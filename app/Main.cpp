// Analog '83 preview app: the kit page at KICK's 1024 x 600 screen size.
#include "MainComponent.h"

class Analog83Application : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise(const juce::String&) override { mainWindow = std::make_unique<MainWindow>(getApplicationName()); }
    void shutdown() override                       { mainWindow = nullptr; }
    void systemRequestedQuit() override            { quit(); }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name, a83ui::colours::background, DocumentWindow::closeButton | DocumentWindow::minimiseButton)
        {
            setUsingNativeTitleBar(true);
            auto* content = new a83ui::MainComponent();
            setContentOwned(content, true);
            setResizable(false, false);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
            content->grabKeyboardFocus();
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(Analog83Application)
