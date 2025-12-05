#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

//==============================================================================
// Constructor / destructor
ArpeggiatorPluginAudioProcessor::ArpeggiatorPluginAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "PARAMS", createParameterLayout())
#else
     : apvts (*this, nullptr, "PARAMS", createParameterLayout())
#endif
{
    // Defaults
    currentSampleRate    = 44100.0;
    bpm                  = 120.0;
    division             = 16;     // semicorcheas
    samplesPerStep       = 1;
    samplesUntilNextStep = 1;

    direction            = ArpDirection::Up;
    goingUp              = true;
}

ArpeggiatorPluginAudioProcessor::~ArpeggiatorPluginAudioProcessor() = default;

//==============================================================================
// Parámetros (APVTS)
ArpeggiatorPluginAudioProcessor::APVTS::ParameterLayout
ArpeggiatorPluginAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    using namespace juce;

    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "DIVISION", 1 },
        "Division",
        StringArray { "1/4", "1/8", "1/16", "1/32" },
        2   // default: "1/16"
    ));

    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "DIRECTION", 1 },
        "Direction",
        StringArray { "Up", "Down", "UpDown", "Random" },
        0   // default: Up
    ));

    return { params.begin(), params.end() };
}

// Helpers estáticos para mapear índice -> valor real
int ArpeggiatorPluginAudioProcessor::divisionFromIndex (int index)
{
    switch (index)
    {
        case 0:  return 4;   // 1/4
        case 1:  return 8;   // 1/8
        case 2:  return 16;  // 1/16
        case 3:  return 32;  // 1/32
        default: return 16;
    }
}

ArpeggiatorPluginAudioProcessor::ArpDirection
ArpeggiatorPluginAudioProcessor::directionFromIndex (int index)
{
    switch (index)
    {
        case 0:  return ArpDirection::Up;
        case 1:  return ArpDirection::Down;
        case 2:  return ArpDirection::UpDown;
        case 3:  return ArpDirection::Random;
        default: return ArpDirection::Up;
    }
}

//==============================================================================
// Información básica del plugin
const juce::String ArpeggiatorPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ArpeggiatorPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ArpeggiatorPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ArpeggiatorPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ArpeggiatorPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ArpeggiatorPluginAudioProcessor::getNumPrograms()          { return 1; }
int ArpeggiatorPluginAudioProcessor::getCurrentProgram()       { return 0; }
void ArpeggiatorPluginAudioProcessor::setCurrentProgram (int)  {}
const juce::String ArpeggiatorPluginAudioProcessor::getProgramName (int) { return {}; }
void ArpeggiatorPluginAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
// Preparación / liberación
void ArpeggiatorPluginAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    updateTimingFromHost(); // intenta leer BPM del host
    updateTimingFromBpm();  // recalcula muestras por paso
}

void ArpeggiatorPluginAudioProcessor::releaseResources()
{
}

//==============================================================================
// Layout de buses
#ifndef JucePlugin_PreferredChannelConfigurations
bool ArpeggiatorPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // stereo / mono
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

//==============================================================================
// Helpers de timing
void ArpeggiatorPluginAudioProcessor::updateTimingFromHost()
{
    auto* playHead = getPlayHead();
    if (playHead == nullptr)
        return;

    juce::AudioPlayHead::CurrentPositionInfo pos;
    if (playHead->getCurrentPosition (pos))
    {
        if (pos.bpm > 0.0)
            bpm = pos.bpm;
    }
}

void ArpeggiatorPluginAudioProcessor::updateTimingFromBpm()
{
    if (currentSampleRate <= 0.0)
        return;

    // Leer parámetros desde APVTS
    if (auto* divParam = apvts.getRawParameterValue ("DIVISION"))
        division = divisionFromIndex ((int) std::round (divParam->load()));
    
    if (auto* dirParam = apvts.getRawParameterValue ("DIRECTION"))
        direction = directionFromIndex ((int) std::round (dirParam->load()));

    // beat = negra: duración en segundos
    const double beatDurationSec  = 60.0 / bpm;
    // stepDuration = 1/division nota
    // 4/4 = 1 (beat) | 4/16 = 0.25 de beat
    const double stepDurationSec  = beatDurationSec * (4.0 / (double) division);
    
    // Convertir segundos a samples
    samplesPerStep = (int) std::max (1.0, std::round (stepDurationSec * currentSampleRate));

    //samplesUntilNextStep es un contador que se va decrementando en el rendering del audio
    if (samplesUntilNextStep <= 0 || samplesUntilNextStep > samplesPerStep)
        samplesUntilNextStep = samplesPerStep;
}

//==============================================================================
// Helpers de notas
void ArpeggiatorPluginAudioProcessor::turnOffCurrentNote (juce::MidiBuffer& midiOut, int samplePos)
{
    if (currentNote >= 0)
    {
        auto off = juce::MidiMessage::noteOff (currentChannel, currentNote);
        midiOut.addEvent (off, samplePos);

        currentNote      = -1;
        currentVelocity  = 0;
    }
}

void ArpeggiatorPluginAudioProcessor::noteOnReceived (int noteNumber, int velocity, int channel,
                                                      juce::MidiBuffer& midiOut, int samplePos)
{
    // Evitar duplicados exactos
    auto it = std::find_if (heldNotes.begin(), heldNotes.end(),
                            [noteNumber, channel] (const HeldNote& n)
                            {
                                return n.noteNumber == noteNumber && n.channel == channel;
                            });

    if (it == heldNotes.end())
    {
        HeldNote n;
        n.noteNumber = noteNumber;
        n.velocity   = velocity;
        n.channel    = channel;
        heldNotes.push_back (n);

        // Ordenar por altura cada vez que agregamos una nueva nota
        sortHeldNotes();
    }

    // Si no hay nota actual sonando, disparar cuanto antes
    if (currentNote < 0)
        samplesUntilNextStep = 0;

    juce::ignoreUnused (midiOut, samplePos);
}

void ArpeggiatorPluginAudioProcessor::noteOffReceived (int noteNumber, int channel,
                                                       juce::MidiBuffer& midiOut, int samplePos)
{
    heldNotes.erase (std::remove_if (heldNotes.begin(), heldNotes.end(),
                                     [noteNumber, channel] (const HeldNote& n)
                                     {
                                         return n.noteNumber == noteNumber && n.channel == channel;
                                     }),
                     heldNotes.end());

    if (currentNote == noteNumber && currentChannel == channel)
        turnOffCurrentNote (midiOut, samplePos);

    if (heldNotes.empty())
    {
        currentNoteIndex = -1;
    }
    else
    {
        // Reordenar y recalcular índice
        sortHeldNotes();
    }
}

void ArpeggiatorPluginAudioProcessor::sortHeldNotes()
{
    // Ordenar por altura (noteNumber). Si hay empate, por canal.
    std::sort (heldNotes.begin(), heldNotes.end(),
               [] (const HeldNote& a, const HeldNote& b)
               {
                   if (a.noteNumber == b.noteNumber)
                       return a.channel < b.channel;

                   return a.noteNumber < b.noteNumber;
               });

    // Recalcular currentNoteIndex para que siga apuntando a la nota actual
    if (currentNote < 0 || heldNotes.empty())
    {
        currentNoteIndex = -1;
        return;
    }

    for (int i = 0; i < (int) heldNotes.size(); ++i)
    {
        const auto& n = heldNotes[(size_t) i];
        if (n.noteNumber == currentNote && n.channel == currentChannel)
        {
            currentNoteIndex = i;
            return;
        }
    }

    // Si la nota actual ya no está en la lista, reseteamos el índice
    currentNoteIndex = -1;
}

//==============================================================================
// Selección de índice según dirección
int ArpeggiatorPluginAudioProcessor::getNextIndex()
{
    if (heldNotes.empty())
        return -1;

    const int size = (int) heldNotes.size();

    if (currentNoteIndex < 0 || currentNoteIndex >= size)
        currentNoteIndex = 0;

    switch (direction)
    {
        case ArpDirection::Up:
            currentNoteIndex = (currentNoteIndex + 1) % size;
            return currentNoteIndex;

        case ArpDirection::Down:
            currentNoteIndex--;
            if (currentNoteIndex < 0)
                currentNoteIndex = size - 1;
            return currentNoteIndex;

        case ArpDirection::UpDown:
        {
            if (goingUp)
            {
                currentNoteIndex++;
                if (currentNoteIndex >= size - 1)
                {
                    currentNoteIndex = size - 1;
                    goingUp = false;
                }
            }
            else
            {
                currentNoteIndex--;
                if (currentNoteIndex <= 0)
                {
                    currentNoteIndex = 0;
                    goingUp = true;
                }
            }
            return currentNoteIndex;
        }

        case ArpDirection::Random:
            currentNoteIndex = rng.nextInt (size);
            return currentNoteIndex;

        default:
            break;
    }

    return currentNoteIndex;
}

//==============================================================================
void ArpeggiatorPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // No procesamos audio: lo dejamos tal cual
    juce::ignoreUnused (buffer);

    const int numSamples = buffer.getNumSamples();

    updateTimingFromHost(); // Actualiza BPM
    updateTimingFromBpm(); // Actualiza samplesUntilNextStep

    juce::MidiBuffer processedMidi;

    // 2) Procesar MIDI entrante: construir heldNotes y pasar mensajes no NOTE
    for (const auto metadata : midiMessages)
    {
        const auto& msg     = metadata.getMessage();
        const int samplePos = metadata.samplePosition;

        if (msg.isNoteOn())
        {
            noteOnReceived (msg.getNoteNumber(),
                            (int) msg.getVelocity(),
                            msg.getChannel(),
                            processedMidi,
                            samplePos);
        }
        else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
        {
            noteOffReceived (msg.getNoteNumber(),
                             msg.getChannel(),
                             processedMidi,
                             samplePos);
        }
        else
        {
            // CC, pitch bend, aftertouch, etc. pasan directo
            processedMidi.addEvent (msg, samplePos);
        }
    }

    // Limpiamos el buffer MIDI original para reconstruirlo
    midiMessages.clear();

    // 3) Avanzar el "reloj" por muestra y disparar notas arpegiadas
    for (int i = 0; i < numSamples; ++i)
    {
        samplesUntilNextStep -= 1;
        
        if (samplesUntilNextStep <= 0) // ¿Toca disparar un nuevo step?
        {
            if (! heldNotes.empty()) // Hay notas siendo tocadas para arpegiar?
            {
                const int nextIndex = getNextIndex();

                if (nextIndex >= 0)
                {
                    const auto& next = heldNotes[(size_t) nextIndex];

                    // Apagar nota anterior (si hay)
                    turnOffCurrentNote (processedMidi, i);

                    // Disparar nueva nota
                    currentNote     = next.noteNumber;
                    currentChannel  = next.channel;
                    currentVelocity = next.velocity;

                    auto on = juce::MidiMessage::noteOn (currentChannel,
                                                         currentNote,
                                                         (juce::uint8) currentVelocity);
                    // Insertar el mensaje MIDI on en el buffer processedMidi para ejecutarse en la muestra i del bloque de audio actual:
                    processedMidi.addEvent (on, i);

                    samplesUntilNextStep = samplesPerStep;
                }
            }
            else
            {
                // Sin notas sostenidas, reseteamos el counter hasta el proximo step
                samplesUntilNextStep = samplesPerStep;
            }
        }
    }

    // 4) Devolvemos el MIDI procesado al host
    midiMessages.swapWith (processedMidi);
}

//==============================================================================
// Editor
bool ArpeggiatorPluginAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ArpeggiatorPluginAudioProcessor::createEditor()
{
    return new ArpeggiatorPluginAudioProcessorEditor (*this);
}

//==============================================================================
// Estado (guardar/recuperar parámetros)
void ArpeggiatorPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void ArpeggiatorPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (tree.isValid())
        apvts.replaceState (tree);
}

//==============================================================================
// Factory del plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ArpeggiatorPluginAudioProcessor();
}
