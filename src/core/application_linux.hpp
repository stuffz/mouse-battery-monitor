#pragma once

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QSocketNotifier>
#include <QString>
#include <QSystemTrayIcon>
#include <QTimer>

#include <fcntl.h>
#include <libudev.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "battery_monitor.hpp"
#include "config.hpp"
#include "device_manager.hpp"
#include "hotplug_scheduler.hpp"
#include "logger.hpp"
#include "platform/paths_linux.hpp"
#include "platform/single_instance.hpp"
#include "ui/icon_loader.hpp"
#include "ui/notification_manager.hpp"
#include "ui/tray_icon.hpp"

class Application
{
public:
    static Application &instance()
    {
        static Application app;
        return app;
    }

    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;

    void setBuildInfo(const char *date, const char *hash)
    {
        buildDate = date;
        gitHash = hash;
    }

    int run(int argc, char **argv)
    {
        const Options options = parseArgs(argc, argv);

        switch (options.action)
        {
        case Action::ShowHelp:
            printHelp(argv[0]);
            return options.unknownArg ? 1 : 0;
        case Action::ShowVersion:
            printVersion();
            return 0;
        case Action::ReadOnce:
            loadConfig(options.forceDebug);
            return runOnce();
        case Action::RunTray:
            loadConfig(options.forceDebug);
            return runTray(argc, argv);
        }

        return 1;
    }

private:
    Application() = default;

    enum class Action
    {
        RunTray,
        ReadOnce,
        ShowHelp,
        ShowVersion
    };

    struct Options
    {
        Action action = Action::RunTray;
        bool forceDebug = false;
        bool unknownArg = false;
    };

    Config config;
    IconLoader iconLoader;
    TrayIcon trayIcon;
    NotificationManager notificationManager;
    BatteryMonitor batteryMonitor;

    std::unique_ptr<QMenu> contextMenu;
    std::unique_ptr<QTimer> updateTimer;
    std::unique_ptr<QTimer> deviceChangeTimer;
    std::unique_ptr<QTimer> arrivalRetryTimer;
    std::unique_ptr<QSocketNotifier> udevNotifier;
    std::unique_ptr<QSocketNotifier> signalNotifier;
    SingleInstanceLock instanceLock;

    udev *udevContext = nullptr;
    udev_monitor *udevMonitor = nullptr;
    int signalFd = -1;
    sigset_t terminationMask{};
    bool signalsBlocked = false;

    HotplugScheduler hotplug;

    bool debugMode = false;

    const char *buildDate = "unknown";
    const char *gitHash = "unknown";

    static Options parseArgs(int argc, char **argv)
    {
        Options options;

        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];

            if (arg == "--once" || arg == "-1")
            {
                options.action = Action::ReadOnce;
            }
            else if (arg == "--debug" || arg == "-d")
            {
                options.forceDebug = true;
            }
            else if (arg == "--version" || arg == "-v")
            {
                options.action = Action::ShowVersion;
            }
            else if (arg == "--help" || arg == "-h")
            {
                options.action = Action::ShowHelp;
                return options;
            }
            else
            {
                std::cerr << "Unrecognised option: " << arg << "\n\n";
                options.action = Action::ShowHelp;
                options.unknownArg = true;
                return options;
            }
        }

        return options;
    }

    static void printHelp(const char *program)
    {
        std::cout
            << "Mouse Battery Monitor - tray battery monitor for Endgame Gear and VAXEE mice\n\n"
            << "Usage: " << program << " [options]\n\n"
            << "Options:\n"
            << "  -1, --once      Print battery status once and exit (no tray icon)\n"
            << "  -d, --debug     Enable verbose logging to stdout and the log file\n"
            << "  -v, --version   Print build information and exit\n"
            << "  -h, --help      Show this help\n\n"
            << "Files:\n"
            << "  Config:    " << paths::ConfigPath().string() << "\n"
            << "  Log:       " << paths::StateDir().string() << "/battery_monitor.log\n"
            << "  Resources: " << paths::ResourceDir().string() << "\n";
    }

    void printVersion() const
    {
        std::cout << "Mouse Battery Monitor\n"
                  << "Build Date: " << buildDate << "\n"
                  << "Git Hash: " << gitHash << "\n";
    }

    static std::string toUtf8(const std::wstring &text)
    {
        return QString::fromStdWString(text).toStdString();
    }

    void loadConfig(bool forceDebug)
    {
        const auto configPath = paths::ConfigPath();

        const bool loaded = config.Load(configPath.string());

        debugMode = config.GetDebugMode() || forceDebug;

        Logger::Instance().SetDebugMode(debugMode);
        Logger::Instance().SetLogFile(paths::LogPath().string());

        if (loaded)
        {
            LOG_DEBUG("Loaded configuration from " + configPath.string());
        }
        else
        {
            LOG_ERROR("Failed to load " + configPath.string() + ", using defaults");
        }

        LOG_DEBUG("Logger configured");
    }

    int runOnce()
    {
        DeviceManager devices;

        if (!devices.FindAndConnect())
        {
            std::cout << "No supported device found\n";
            return 1;
        }

        const auto status = devices.ReadBattery();
        const std::string name = toUtf8(devices.GetDeviceName());
        const std::string mode = toUtf8(devices.GetConnectionMode());

        devices.Disconnect();

        if (status.percentage < 0)
        {
            std::cout << name << " (" << mode << "): no reading (device asleep?)\n";
            return 1;
        }

        std::cout << name << " (" << mode << "): " << status.percentage << "%"
                  << (status.isCharging ? " charging" : "") << "\n";
        return 0;
    }

    int runTray(int argc, char **argv)
    {
        // --once stays unguarded: a status bar may poll while the tray runs.
        if (!acquireInstanceLock())
        {
            return 0;
        }

        // Qt's worker threads inherit this mask, so it has to be installed
        // first: otherwise a process-directed SIGTERM lands on an unblocked Qt
        // thread and kills us before shutdown() can run.
        blockTerminationSignals();

        // KDE's platform theme supplies a native message-dialog helper that
        // segfaults in QDialogPrivate::setNativeDialogVisible when the dialog is
        // dismissed by a close request (the titlebar X) rather than a button.
        // Qt's own widget dialogs are unaffected.
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);

        QApplication qtApp(argc, argv);
        QApplication::setApplicationName(QStringLiteral("Mouse Battery Monitor"));
        QApplication::setDesktopFileName(QStringLiteral("mouse-battery-monitor"));

        QApplication::setQuitOnLastWindowClosed(false);

        LOG_INFO("Starting Mouse Battery Monitor");

        if (!QSystemTrayIcon::isSystemTrayAvailable())
        {
            LOG_ERROR("No system tray detected - the icon will appear once a tray host starts");
            std::cerr << "Warning: no system tray detected. On GNOME you need an "
                         "AppIndicator extension.\n";
        }

        if (!loadResources())
        {
            return 1;
        }

        setupComponents();
        setupHotplugMonitor();
        setupSignalHandler();

        batteryMonitor.update();

        const int result = qtApp.exec();
        shutdown();
        return result;
    }

    bool acquireInstanceLock()
    {
        const auto lockPath = paths::LockPath();

        switch (instanceLock.acquire(lockPath))
        {
        case SingleInstanceLock::Result::Acquired:
            LOG_DEBUG("Instance lock acquired: " + lockPath.string());
            return true;

        case SingleInstanceLock::Result::AlreadyRunning:
        {
            const std::string &pid = instanceLock.holderPid();
            const std::string suffix = pid.empty() ? std::string() : " (pid " + pid + ")";

            LOG_INFO("Another instance is already running" + suffix + " - exiting");
            std::cerr << "Mouse Battery Monitor is already running" << suffix << ".\n";

            // Exit 0, not failure: a non-zero exit would send a
            // Restart=on-failure unit into a loop on every double-start.
            return false;
        }

        case SingleInstanceLock::Result::Unavailable:
            LOG_ERROR("Instance lock unavailable - continuing without a single-instance guard");
            return true;
        }

        return true;
    }

    bool loadResources()
    {
        const auto resourceDir = paths::ResourceDir();
        LOG_DEBUG("Resource directory: " + resourceDir.string());

        if (!iconLoader.LoadIcons(resourceDir))
        {
            LOG_ERROR("Failed to load icons from " + resourceDir.string());
            QMessageBox::critical(nullptr, QStringLiteral("Error"),
                                  QStringLiteral("Failed to load icon resources from\n%1\n\n"
                                                 "Set MBM_RESOURCE_DIR to the directory holding "
                                                 "the battery PNG files.")
                                      .arg(QString::fromStdString(resourceDir.string())));
            iconLoader.Clear();
            return false;
        }

        LOG_DEBUG("Icon load succeeded");
        return true;
    }

    void setupComponents()
    {
        trayIcon.init(iconLoader.GetDisconnectedIcon(),
                      L"Mouse Battery Monitor\nNo device connected");
        buildContextMenu();

        notificationManager.setTrayIcon(&trayIcon);
        notificationManager.setThreshold(config.GetLowBatteryThreshold());
        notificationManager.setEnabled(config.GetShowNotifications());

        batteryMonitor.init(&trayIcon, &iconLoader, &notificationManager);

        updateTimer = std::make_unique<QTimer>();
        updateTimer->setInterval(config.GetUpdateIntervalSeconds() * 1000);
        QObject::connect(updateTimer.get(), &QTimer::timeout, [this] { batteryMonitor.update(); });
        updateTimer->start();
        LOG_DEBUG("Update timer set for " + std::to_string(config.GetUpdateIntervalSeconds()) +
                  " seconds");

        deviceChangeTimer = std::make_unique<QTimer>();
        deviceChangeTimer->setSingleShot(true);
        QObject::connect(deviceChangeTimer.get(), &QTimer::timeout,
                         [this] { hotplug.onDebounceElapsed(); });

        arrivalRetryTimer = std::make_unique<QTimer>();
        arrivalRetryTimer->setInterval(HotplugScheduler::ARRIVAL_RETRY_MS);
        QObject::connect(arrivalRetryTimer.get(), &QTimer::timeout,
                         [this] { hotplug.onRetryElapsed(); });

        hotplug.init(&batteryMonitor,
                     {[this](int delayMs) { deviceChangeTimer->start(delayMs); }, [this]
                      { arrivalRetryTimer->start(); }, [this] { arrivalRetryTimer->stop(); }});
    }

    void buildContextMenu()
    {
        contextMenu = std::make_unique<QMenu>();

        QAction *update = contextMenu->addAction(QStringLiteral("Update Now"));
        QObject::connect(update, &QAction::triggered, [this] { batteryMonitor.update(); });

        if (debugMode)
        {
            QAction *test = contextMenu->addAction(QStringLiteral("Trigger Low Battery"));
            QObject::connect(
                test, &QAction::triggered, [this]
                { batteryMonitor.triggerTestNotification(config.GetLowBatteryThreshold()); });
        }

        contextMenu->addSeparator();

        QAction *about = contextMenu->addAction(QStringLiteral("About"));
        QObject::connect(about, &QAction::triggered, [this] { showAboutDialog(); });

        QAction *exit = contextMenu->addAction(QStringLiteral("Exit"));
        QObject::connect(exit, &QAction::triggered, [] { QCoreApplication::quit(); });

        trayIcon.setContextMenu(contextMenu.get());
    }

    void setupHotplugMonitor()
    {
        udevContext = udev_new();
        if (!udevContext)
        {
            LOG_ERROR("udev_new failed - hotplug detection disabled");
            return;
        }

        udevMonitor = udev_monitor_new_from_netlink(udevContext, "udev");
        if (!udevMonitor)
        {
            LOG_ERROR("udev_monitor_new_from_netlink failed - hotplug detection disabled");
            return;
        }

        udev_monitor_filter_add_match_subsystem_devtype(udevMonitor, "hidraw", nullptr);

        if (udev_monitor_enable_receiving(udevMonitor) < 0)
        {
            LOG_ERROR("udev_monitor_enable_receiving failed - hotplug detection disabled");
            return;
        }

        const int fd = udev_monitor_get_fd(udevMonitor);
        const int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags >= 0)
        {
            ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }

        udevNotifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read);
        QObject::connect(udevNotifier.get(), &QSocketNotifier::activated,
                         [this] { onUdevEvent(); });

        LOG_DEBUG("USB device notifications registered (udev hidraw monitor)");
    }

    void onUdevEvent()
    {
        // Tracked locally so an unrelated event cannot re-arm a pending debounce.
        HotplugScheduler::Event seen = HotplugScheduler::Event::None;

        while (udev_device *device = udev_monitor_receive_device(udevMonitor))
        {
            if (const char *action = udev_device_get_action(device))
            {
                if (std::strcmp(action, "add") == 0)
                {
                    seen = HotplugScheduler::Event::Arrival;
                }
                else if (std::strcmp(action, "remove") == 0)
                {
                    seen = HotplugScheduler::Event::Removal;
                }
            }
            udev_device_unref(device);
        }

        hotplug.postEvent(seen);
    }

    void blockTerminationSignals()
    {
        sigemptyset(&terminationMask);
        sigaddset(&terminationMask, SIGINT);
        sigaddset(&terminationMask, SIGTERM);

        if (pthread_sigmask(SIG_BLOCK, &terminationMask, nullptr) != 0)
        {
            LOG_ERROR("pthread_sigmask failed - signals will not shut down cleanly");
            return;
        }

        signalsBlocked = true;
    }

    void setupSignalHandler()
    {
        if (!signalsBlocked)
        {
            return;
        }

        signalFd = signalfd(-1, &terminationMask, SFD_NONBLOCK | SFD_CLOEXEC);
        if (signalFd < 0)
        {
            LOG_ERROR("signalfd failed - signals will not shut down cleanly");
            return;
        }

        signalNotifier = std::make_unique<QSocketNotifier>(signalFd, QSocketNotifier::Read);
        QObject::connect(signalNotifier.get(), &QSocketNotifier::activated,
                         [this]
                         {
                             signalfd_siginfo info;
                             while (::read(signalFd, &info, sizeof(info)) == sizeof(info))
                             {
                             }
                             LOG_INFO("Termination signal received");
                             QCoreApplication::quit();
                         });
    }

    void showAboutDialog()
    {
        std::ostringstream ss;
        ss << "Mouse Battery Monitor\n\n"
           << "Build Date: " << buildDate << "\n"
           << "Git Hash: " << gitHash << "\n\n"
           << "Monitors battery status for Endgame Gear and VAXEE wireless mice.\n\n"
           << "Update Interval: " << config.GetUpdateIntervalSeconds() << " seconds\n"
           << "Low Battery Threshold: " << config.GetLowBatteryThreshold() << "%\n"
           << "Debug Mode: " << (debugMode ? "Enabled" : "Disabled");

        QMessageBox::information(nullptr, QStringLiteral("About Mouse Battery Monitor"),
                                 QString::fromStdString(ss.str()));
    }

    void shutdown()
    {
        // Runs after exec() returned, so nothing can fire; QApplication must
        // still outlive these.
        udevNotifier.reset();
        signalNotifier.reset();

        trayIcon.remove();
        contextMenu.reset();
        updateTimer.reset();
        deviceChangeTimer.reset();
        arrivalRetryTimer.reset();
        iconLoader.Clear();

        if (signalFd >= 0)
        {
            ::close(signalFd);
            signalFd = -1;
        }
        if (udevMonitor)
        {
            udev_monitor_unref(udevMonitor);
            udevMonitor = nullptr;
        }
        if (udevContext)
        {
            udev_unref(udevContext);
            udevContext = nullptr;
        }

        batteryMonitor.devices().Disconnect();
        LOG_INFO("Shutting down");
    }
};
