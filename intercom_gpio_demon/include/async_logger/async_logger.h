#ifndef ASYNC_LOGGER_HPP
#define ASYNC_LOGGER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <ctime>
#include <queue>
#include <thread>
#include <condition_variable>
#include <filesystem>
#include <map>
#include <algorithm>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

namespace fs = std::filesystem;

/**
 * @brief Log message severity filtering levels.
 */
enum class LogLevel { DEBUG = 0, INFO = 1, ERROR = 2, OFF = 3 };

/**
 * @brief Queue behavior policies when maximum capacity is reached.
 */
enum class OverflowPolicy { BLOCK, DROP_NEWEST };

#ifndef LOG_COMPILE_LEVEL
#define LOG_COMPILE_LEVEL 0
#endif

/**
 * @brief Multi-threaded, asynchronous logger class (Singleton).
 *
 * Manages message queuing in a FIFO queue and handles asynchronous
 * dispatching to multiple output sinks (Console, Rotating File, UDP Network).
 */
class AsyncLogger
{
public:

	/**
	 * @brief Gets the singleton instance of the logger.
	 * @return Reference to the static AsyncLogger instance.
	 */
    static AsyncLogger& instance()
    {
        static AsyncLogger inst;
        return inst;
    }

    /**
     * @brief Loads configuration from an INI file and applies settings immediately.
     *
     * If the specified file does not exist, a default template is created automatically
     * (net=true, level=ERROR, net_port=5000, net_timeout_sec=0).
     *
     * @param ini_path Path to the INI configuration file (defaults to "logger.ini").
     * @return true if the configuration was successfully loaded or created; false on file error.
     */
    bool load_config_ini(const std::string& ini_path = "logger.ini")
    {
        if (!fs::exists(ini_path))
        {
            std::cout << "[AsyncLogger] File not found " << ini_path
                      << "Creating the default configuration(net=true, level=ERROR, port=5000)\n";
            create_default_ini(ini_path);
        }

        std::ifstream in(ini_path);
        if (!in.is_open())
        {
            std::cerr << "[AsyncLogger] Error opening INI file: " << ini_path << "\n";
            return false;
        }

        std::map<std::string, std::string> config;
        std::string line;

        while (std::getline(in, line))
        {
            auto comment_pos = line.find_first_of("#;");
            if (comment_pos != std::string::npos)
            {
                line = line.substr(0, comment_pos); // @suppress("Invalid arguments")
            }

            line = trim(line);
            if (line.empty() || line[0] == '[')
            {
            	continue;
            }

            auto eq_pos = line.find('=');
            if (eq_pos != std::string::npos)
            {
                std::string key = trim((const std::string&)line.substr(0, eq_pos)); // @suppress("Invalid arguments")
                std::string val = trim((const std::string&)line.substr(eq_pos + 1)); // @suppress("Invalid arguments")

                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                config[key] = val;
            }
        }
        in.close();

        if (config.count("level"))
        {
            set_runtime_level(parse_level(config["level"]));
        }

        bool term_en = (config["terminal"] == "true" || config["terminal"] == "1");
        enable_console(term_en, true);

        bool file_en = (config["file"] == "true" || config["file"] == "1");
        if (file_en)
        {
            std::string path = config.count("filepath") ? config["filepath"] : "app.log";
            open_file(path);
        }
        else
        {
            close_file();
        }

        bool net_en = (config["net"] == "true" || config["net"] == "1");
        if (net_en)
        {
            uint16_t port = 5000;
            int net_timeout_sec = 0;

            if (config.count("net_port"))
            {
                try
                {
                    port = static_cast<uint16_t>(std::stoi(config["net_port"]));
                }
                catch (...)
                {
                    port = 5000;
                }
            }
            if (config.count("net_timeout_sec"))
            {
            	try
                {
                	net_timeout_sec = std::stoi(config["net_timeout_sec"]);
                }
                catch (...)
                {
                	net_timeout_sec = 0;
                }
            }
        }
        else
        {
            disable_network();
        }
        return true;
    }

    /**
     * @brief Configures internal queue bounds and log file rotation parameters.
     *
     * @param max_queue_size Maximum capacity of the lock-free/guarded queue.
     * @param policy Overflow handling strategy (BLOCK or DROP_NEWEST).
     * @param max_file_size_bytes Maximum file size in bytes before triggering rotation.
     * @param max_backup_files Maximum number of historical backup files to retain (e.g., app.log.1).
     */
    void configure_queue(std::size_t max_queue_size = 10000, OverflowPolicy policy = OverflowPolicy::BLOCK,
    					 std::size_t max_file_size_bytes = 10 * 1024 * 1024, std::size_t max_backup_files = 5)
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        max_queue_size_ = max_queue_size;
        policy_ = policy;
        max_file_size_bytes_ = max_file_size_bytes;
        max_backup_files_ = max_backup_files;
    }

    /**
     * @brief Dynamically updates the minimum runtime logging level.
     *
     * Messages lower than this severity level are discarded immediately without queuing.
     *
     * @param level New minimum runtime LogLevel threshold.
     */
    void set_runtime_level(LogLevel level)
    {
        runtime_level_.store(level, std::memory_order_relaxed);
    }

    /**
     * @brief Formats a log entry with a timestamp and pushes it into the async queue.
     *
     * @param level Severity level of the log message.
     * @param message Text payload of the log message.
     */
    void log(LogLevel level, std::string message)
    {
        if (level < runtime_level_.load(std::memory_order_relaxed) || runtime_level_ == LogLevel::OFF)
        {
            return;
        }

        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
        localtime_r(&time_t_now, &tm_buf);

        std::ostringstream time_ss;
        time_ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");

        const char* level_str = "";
        switch (level)
        {
            case LogLevel::DEBUG: level_str = "DEBUG";
            	break;
            case LogLevel::INFO:  level_str = "INFO";
            	break;
            case LogLevel::ERROR: level_str = "ERROR";
            	break;
            default:
            	break;
        }

        std::string formatted = "[" + time_ss.str() + "] [" + level_str + "] " + message + "\n";

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            if (policy_ == OverflowPolicy::BLOCK)
            {
                cv_producer_.wait(lock, [this] {return queue_.size() < max_queue_size_ || !is_running_;});
                if (!is_running_)
                {
                	return;
                }
                queue_.push({level, std::move(formatted)});
            } 
            else {
                if (queue_.size() >= max_queue_size_)
                {
                    dropped_count_.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
                queue_.push({level, std::move(formatted)});
            }
        }
        cv_consumer_.notify_one();
    }

private:
    std::ofstream file_;
    std::string base_filename_;
    std::size_t current_file_size_{0};
	std::size_t max_file_size_bytes_{10 * 1024 * 1024};
	std::size_t max_backup_files_{5};

    bool console_enabled_{false};
    bool console_colors_{true};

    std::atomic<bool> network_enabled_{false};
    std::atomic<int> net_socket_{-1};
    sockaddr_in net_addr_{};

    struct LogEntry
	{
        LogLevel level;
        std::string formatted_message;
    };

    std::queue<LogEntry> queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_consumer_;
    std::condition_variable cv_producer_;

    std::size_t max_queue_size_{10000};
    OverflowPolicy policy_{OverflowPolicy::BLOCK};
    std::atomic<uint64_t> dropped_count_{0};

    bool is_running_{true};
    std::atomic<LogLevel> runtime_level_{LogLevel::DEBUG};
    std::thread worker_thread_;

    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

    AsyncLogger() : is_running_(true)
    {
        worker_thread_ = std::thread(&AsyncLogger::process_queue, this);
    }

    ~AsyncLogger()
    {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            is_running_ = false;
        }
        cv_consumer_.notify_all();
        cv_producer_.notify_all();

        if (worker_thread_.joinable())
        {
            worker_thread_.join();
        }

        if (file_.is_open()) file_.close();
        close_network_socket();
    }

    /**
     * @brief Enables or disables logging output to the standard console (stdout).
     *
     * @param enable Whether console output should be active.
     * @param use_colors Whether to use ANSI color escape sequences for log levels.
     */
    void enable_console(bool enable = true, bool use_colors = true)
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        console_enabled_ = enable;
        console_colors_ = use_colors;
    }

    /**
     * @brief Blocks the calling thread and waits for a UDP subscription packet from a client.
     *
     * Upon receiving the expected command, sends an ACK handshake and enables network streaming.
     *
     * @param listen_port Local UDP port to bind and listen on.
     * @param expected_cmd Handshake string expected from the subscriber (e.g., "SUBSCRIBE").
     * @param timeout_seconds Maximum wait time in seconds (0 = block indefinitely).
     * @return true if valid subscription command was received and ACK sent; false on error or timeout.
     */
    bool wait_for_network_subscription(uint16_t listen_port, const std::string& expected_cmd = "SUBSCRIBE", int timeout_seconds = 0)
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        close_network_socket();

        int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock_fd < 0) return false;

        int opt = 1;
        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // @suppress("Invalid arguments")

        sockaddr_in bind_addr{};
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_addr.s_addr = INADDR_ANY;
        bind_addr.sin_port = htons(listen_port);

        if (bind(sock_fd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) // @suppress("Invalid arguments")
        {
        	close(sock_fd);
            return false;
        }

        if (timeout_seconds > 0)
        {
            struct timeval tv;
            tv.tv_sec = timeout_seconds;
            tv.tv_usec = 0;
            setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)); // @suppress("Invalid arguments")
        }

        char buffer[256];
        socklen_t addr_len = sizeof(net_addr_);

        std::cout << "[AsyncLogger] Listening for subscriptions on a port " << listen_port << "...\n";

        ssize_t bytes_received = recvfrom(sock_fd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&net_addr_, &addr_len); // @suppress("Invalid arguments")

        if (bytes_received <= 0)
        {
            std::cerr << "[AsyncLogger] Error or subscription timeout\n";
            close(sock_fd);
            return false;
        }

        buffer[bytes_received] = '\0';
        std::string received_str(buffer);

        std::size_t last_valid = received_str.find_last_not_of(" \n\r\t");
        if (last_valid != std::string::npos)
        {
            received_str = received_str.substr(0, last_valid + 1); // @suppress("Invalid arguments")
        }

        if (!expected_cmd.empty() && received_str != expected_cmd)
        {
            std::cerr << "[AsyncLogger] An invalid command was received: '" << received_str << "' (Expected:'" << expected_cmd << "')\n";
            close(sock_fd);
            return false;
        }

        const char* ack_msg = "ACK: LOG_STREAM_STARTED\n";
        sendto(sock_fd, ack_msg, strlen(ack_msg), 0, (struct sockaddr*)&net_addr_, addr_len); // @suppress("Invalid arguments")

        net_socket_.store(sock_fd);
        network_enabled_.store(true);

        std::cout << "[AsyncLogger] Subscription activated! Log streaming started.\n";
        return true;
    }

    /**
     * @brief Disables network streaming and safely closes the underlying UDP socket.
     */
    void disable_network()
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        close_network_socket();
    }

    /**
     * @brief Opens a log file in append mode.
     *
     * Automatically creates parent directories in the provided path if they do not exist.
     *
     * @param filename Path to the target log file.
     * @return true if the file was opened successfully; false on I/O or permission error.
     */
    bool open_file(const std::string& filename)
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        base_filename_ = filename;

        fs::path p(filename);
        if (p.has_parent_path())
        {
            std::error_code ec;
            fs::create_directories(p.parent_path(), ec);
        }

        if (file_.is_open()) file_.close();
        file_.open(filename, std::ios::out | std::ios::app);

        if (file_.is_open())
        {
            std::error_code ec;
            current_file_size_ = fs::file_size(filename, ec);
            if (ec) current_file_size_ = 0;
            return true;
        }
        return false;
    }

    /**
     * @brief Closes the currently open log file stream.
     */
    void close_file()
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (file_.is_open())
        {
            file_.close();
        }
    }

    /**
     * @brief Writes default configuration values to an INI file on disk.
     * @param ini_path Target path where the INI file should be generated.
     */
    void create_default_ini(const std::string& ini_path)
    {
        std::ofstream out(ini_path);
        if (!out.is_open()) return;

        out << "# AsyncLogger Configuration File\n"
            << "[Logger]\n"
            << "# Logging levels: DEBUG, INFO, ERROR, OFF\n"
            << "level=ERROR\n\n"
            << "# Console output (true / false)\n"
            << "terminal=true\n\n"
            << "# Output to file (true / false)\n"
            << "file=false\n"
            << "filepath=app.log\n\n"
            << "# Ethernet/UDP output (true / false)\n"
            << "net=false\n"
            << "net_port=5000\n"
            << "# Subscription waiting time in seconds (0 = unlimited/blocking)\n"
            << "net_timeout_sec=0\n";

        out.close();
    }

    /**
     * @brief Trims leading and trailing whitespace characters from a string.
     * @param str Input string.
     * @return Sanitized string without whitespace margins.
     */
    static std::string trim(const std::string& str)
    {
        std::size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        std::size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1)); // @suppress("Invalid arguments")
    }

    /**
     * @brief Parses an INI level string (e.g., "DEBUG", "INFO") into a LogLevel enum.
     * @param str Log level name string read from INI.
     * @return Corresponding LogLevel value or LogLevel::OFF if unrecognized.
     */
    static LogLevel parse_level(const std::string& str)
    {
        std::string s = str;
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        if (s == "DEBUG") return LogLevel::DEBUG;
        if (s == "INFO") return LogLevel::INFO;
        if (s == "ERROR") return LogLevel::ERROR;
        return LogLevel::OFF;
    }

    /**
     * @brief Safely closes the socket file descriptor and resets network flags atomically.
     */
    void close_network_socket()
    {
        int fd = net_socket_.exchange(-1);
        network_enabled_.store(false, std::memory_order_relaxed);
        if (fd >= 0) {
            close(fd);
        }
    }

    /**
     * @brief Performs file rotation by shifting backup files and opening a fresh base log file.
     */
    void rotate_file()
    {
        if (file_.is_open()) file_.close();
        std::error_code ec;

        for (std::size_t i = max_backup_files_; i > 0; --i)
        {
            std::string old_name = (i == 1) ? base_filename_ : (base_filename_ + "." + std::to_string((unsigned int)(i - 1)));
            std::string new_name = base_filename_ + "." + std::to_string((unsigned int)i);

            if (fs::exists(old_name, ec))
            {
                if (i == max_backup_files_ && fs::exists(new_name, ec))
                {
                    fs::remove(new_name, ec);
                }
                fs::rename(old_name, new_name, ec);
            }
        }

        file_.open(base_filename_, std::ios::out | std::ios::trunc);
        current_file_size_ = 0;
    }

    /**
     * @brief Main processing loop executed by the background worker thread.
     *
     * Dequeues log batches and writes them to all active sinks.
     */
    void process_queue()
    {
        while (true)
        {
            std::queue<LogEntry> local_queue;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                cv_consumer_.wait(lock, [this] {return !queue_.empty() || !is_running_;});
                if (!is_running_ && queue_.empty()) break;
                local_queue.swap(queue_);
                cv_producer_.notify_all();
            }

            if (!local_queue.empty())
            {
                while (!local_queue.empty())
                {
                    const auto& entry = local_queue.front();
                    const std::string& msg = entry.formatted_message;
                    if (console_enabled_)
                    {
                        if (console_colors_)
                        {
                            const char* color_code = "\033[0m";
                            if (entry.level == LogLevel::DEBUG) color_code = "\033[36m";
                            else if (entry.level == LogLevel::INFO) color_code = "\033[32m";
                            else if (entry.level == LogLevel::ERROR) color_code = "\033[31m";
                            
                            std::cout << color_code << msg << "\033[0m";
                        }
                        else
                        {
                            std::cout << msg;
                        }
                    }

                    int current_sock = net_socket_.load();
                    if (network_enabled_.load() && current_sock >= 0)
                    {
                        sendto(current_sock, msg.c_str(), msg.size(), 0,(struct sockaddr*)&net_addr_, sizeof(net_addr_)); // @suppress("Invalid arguments")
                    }

                    if (file_.is_open())
                    {
                        if (max_file_size_bytes_ > 0 && (current_file_size_ + msg.size()) >= max_file_size_bytes_)
                        {
                            rotate_file();
                        }
                        file_ << msg;
                        current_file_size_ += msg.size();
                    }
                    local_queue.pop();
                }
                if (console_enabled_)
                {
                	std::cout.flush();
                }
                if (file_.is_open())
                {
                	file_.flush();
                }
            }
        }
    }
};


#if LOG_COMPILE_LEVEL <= 0
    /**
     * @brief Logging macro for DEBUG level messages.
     * Completely disabled at compile time if LOG_COMPILE_LEVEL > 0.
     * @param msg Message payload to log.
     */
    #define LOG_DEBUG(msg) AsyncLogger::instance().log(LogLevel::DEBUG, msg)
#else
    #define LOG_DEBUG(msg) do {} while(0)
#endif

#if LOG_COMPILE_LEVEL <= 1
    /**
     * @brief Logging macro for INFO level messages.
     * Completely disabled at compile time if LOG_COMPILE_LEVEL > 1.
     * @param msg Message payload to log.
     */
    #define LOG_INFO(msg) AsyncLogger::instance().log(LogLevel::INFO, msg)
#else
    #define LOG_INFO(msg) do {} while(0)
#endif

#if LOG_COMPILE_LEVEL <= 2
    /**
     * @brief Logging macro for ERROR level messages.
     * Completely disabled at compile time if LOG_COMPILE_LEVEL > 2.
     * @param msg Message payload to log.
     */
    #define LOG_ERROR(msg) AsyncLogger::instance().log(LogLevel::ERROR, msg)
#else
    #define LOG_ERROR(msg) do {} while(0)
#endif

#endif // ASYNC_LOGGER_HPP
