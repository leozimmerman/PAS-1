#pragma once
#include <JuceHeader.h>

/**
 * Consigna:
 * - Agregar un slider con su correspondiente label indicando el valor ("Valor:")
 * - Agregar un botón mostrando un mensaje distinto
 */
class MainComponent final
    : public juce::Component
    , public juce::Slider::Listener
    , public juce::Button::Listener
{
public:
    MainComponent()
    {
        // Titulo (solo lectura/estático)
        titleLabel.setText ("JUCE GUI - Demo simple", juce::dontSendNotification);
        titleLabel.setJustificationType (juce::Justification::centredLeft);
        titleLabel.setFont (juce::Font (18.0f, juce::Font::bold));

        // Slider 0..1
        gainSlider.setRange (0.0, 1.0, 0.01);
        gainSlider.setValue (0.5);
        gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
        gainSlider.addListener (this);

        // Label para mostrar el valor del slider
        valueLabel.setText ("Valor: 0.50", juce::dontSendNotification);
        valueLabel.setJustificationType (juce::Justification::centredLeft);

        // Boton "Acerca de..."
        aboutButton.setButtonText ("Acerca de...");
        aboutButton.addListener (this);

        // Agregar a la UI
        addAndMakeVisible (titleLabel);
        addAndMakeVisible (gainSlider);
        addAndMakeVisible (valueLabel);
        addAndMakeVisible (aboutButton);

        // Tamaño inicial de la ventana
        setSize (480, 220);
    }

    // Dibujo del fondo y separadores simples
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::blue);
        g.setColour (juce::Colours::lightgrey);
        g.drawRect (getLocalBounds(), 1);
    }

    // Layout simple y legible
    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        auto top = area.removeFromTop (28);
        titleLabel.setBounds (top);

        area.removeFromTop (10); // separador

        auto sliderRow = area.removeFromTop (40);
        gainSlider.setBounds (sliderRow.removeFromLeft (area.getWidth() * 2 / 3));
        sliderRow.removeFromLeft (8);
        valueLabel.setBounds (sliderRow);

        area.removeFromTop (10);

        auto buttonRow = area.removeFromTop (36);
        aboutButton.setBounds (buttonRow.removeFromLeft (140));
    }

    // ===== Listeners =====
    void sliderValueChanged (juce::Slider* s) override
    {
        if (s == &gainSlider)
        {
            const auto v = (float) gainSlider.getValue();
            valueLabel.setText ("Valor: " + juce::String (v, 2), juce::dontSendNotification);
        }
    }

    void buttonClicked (juce::Button* b) override
    {
        if (b == &aboutButton)
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::AlertWindow::InfoIcon,
                "Acerca de",
                "Esta es una demo GUI muy simple hecha con JUCE.\n"
                "Controles: Slider, Label y Button."
            );
        }
    }

private:
    juce::Label      titleLabel;
    juce::Slider     gainSlider;
    juce::Label      valueLabel;
    juce::TextButton aboutButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
