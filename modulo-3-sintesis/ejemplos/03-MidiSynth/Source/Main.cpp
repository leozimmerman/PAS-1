// Arranque de la app: crea la ventana y pone adentro el MainComponent.
// Es igual en todos los ejemplos del módulo: lo interesante está en MainComponent.cpp.

#include <JuceHeader.h>
#include "MainComponent.h"

class App : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }

    void initialise (const juce::String&) override { mainWindow.reset (new MainWindow (getApplicationName())); }
    void shutdown() override                       { mainWindow = nullptr; }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                  .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (App)
