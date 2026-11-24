#include "SoulTrainKnob.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace soultrain
{
namespace
{
SoulTrainKnob::Layer loadLayer(const juce::File& directory, const juce::var& metadata)
{
    const auto file = directory.getChildFile(metadata["file"].toString());
    auto image = juce::ImageFileFormat::loadFrom(file);
    if (!image.isValid() || !image.hasAlphaChannel())
        throw std::runtime_error("Missing transparent knob artwork: " + file.getFileName().toStdString());
    return {std::move(image), {float(metadata["x"]), float(metadata["y"])}};
}
}

SoulTrainKnob::Artwork SoulTrainKnob::Artwork::load(const juce::File& directory)
{
    const auto metadata = juce::JSON::parse(directory.getChildFile("cutoff-artwork.json"));
    const auto dialBounds = metadata["dial"];
    if (dialBounds.size()!=4) throw std::runtime_error("Missing or incomplete SoulTrain knob artwork");
    const auto* glowLayers = metadata["glow"].getArray();
    Artwork art;
    art.width = float(metadata["logicalWidth"]);
    art.height = float(metadata["logicalHeight"]);
    art.pixelScale = float(metadata["assetScale"]);
    art.dial = {float(dialBounds[0]), float(dialBounds[1]), float(dialBounds[2]), float(dialBounds[3])};
    if (art.width <= 0 || art.height <= 0 || art.pixelScale <= 0
        || art.dial.getWidth() != 67 || art.dial.getHeight() != 67 || !glowLayers || glowLayers->size() < 2)
        throw std::runtime_error("Invalid SoulTrain knob artwork geometry");

    art.body = loadLayer(directory, metadata["body"]);
    art.focusedBody = loadLayer(directory, metadata["focusedBody"]);
    for (const auto& layer : *glowLayers)
        art.glow.push_back(loadLayer(directory, layer));
   #if JUCE_WINDOWS
    // All knobs with this artwork share the fully lit stretches of the ring.
    // The fractional dot and the marker still follow the exact parameter value.
    const auto prepareGlow = [&](size_t first,size_t end) {
        juce::Image image(juce::Image::ARGB,juce::roundToInt(art.width*art.pixelScale),
                          juce::roundToInt(art.height*art.pixelScale),true);
        juce::Graphics graphics(image);
        for (auto index=first;index<end;++index) {
            const auto& layer=art.glow[index];
            graphics.drawImageTransformed(layer.pixels,juce::AffineTransform::translation(layer.origin*art.pixelScale));
        }
        art.glowFrames.push_back({first,end,{std::move(image),{}}});
    };
    const auto count=art.glow.size();
    for (size_t end=2;end<=count;++end) prepareGlow(0,end);
    for (auto end=count/2+2;end<=count;++end) prepareGlow(count/2,end);
    for (size_t first=1;first+2<=(count+1)/2;++first) prepareGlow(first,(count+1)/2);
   #endif
    return art;
}

SoulTrainKnob::SoulTrainKnob(const Artwork& artwork, Range valueRange, const juce::String& label)
    : juce::Slider(label), art(artwork), range(valueRange)
{
    if (range.maximum <= range.minimum || range.defaultValue < range.minimum || range.defaultValue > range.maximum)
        throw std::invalid_argument("Invalid SoulTrain knob parameter range");
    setSliderStyle(RotaryVerticalDrag);
    setTextBoxStyle(NoTextBox, false, 0, 0);
    setRange(range.minimum, range.maximum, 1);
    setValue(range.defaultValue, juce::dontSendNotification);
    setDoubleClickReturnValue(true, range.defaultValue);
    setWantsKeyboardFocus(true);
    setName(label);
    setTitle(label);

    // Original marker SVG: viewBox origin (22.122, 4.5), rectangle
    // (54.122, 15.5, 3, 18). The viewport stays 67 x 67.
    marker.addRoundedRectangle(32.f, 11.f, 3.f, 18.f, .3f);
}

juce::String SoulTrainKnob::getTextFromValue(double value)
{
    const auto shown = juce::roundToInt(value) + range.offset;
    return (range.isBipolar() && shown > 0 ? "+" : "") + juce::String(shown);
}

double SoulTrainKnob::getValueFromText(const juce::String& text)
{
    return text.trim().getIntValue() - range.offset;
}

double SoulTrainKnob::position() const noexcept
{
    const auto raw = getValue();
    if (!range.isBipolar())
        return (raw - range.minimum) / (range.maximum - range.minimum);

    const double shown = raw + range.offset;
    const double negativeSpan = -(range.minimum + range.offset);
    const double positiveSpan = range.maximum + range.offset;
    return shown / (shown < 0 ? negativeSpan : positiveSpan);
}

double SoulTrainKnob::angleDegrees() const noexcept
{
    const auto relative = position();
    return range.isBipolar() ? 135.0 * relative : -135.0 + 270.0 * relative;
}

double SoulTrainKnob::glowLevel(size_t dotIndex) const noexcept
{
    return glowLevel(dotIndex,position());
}

double SoulTrainKnob::glowLevel(size_t dotIndex,double relative) const noexcept
{
    if (relative == 0) return 0;

    const auto lastDot = double(art.glow.size() - 1);
    if (!range.isBipolar())
        return juce::jlimit(0.0, 1.0, relative * lastDot - double(dotIndex) + 1.0);

    const auto middle = lastDot / 2;
    const auto direction = double(dotIndex) - middle;
    if (direction * relative < 0) return 0;
    return juce::jlimit(0.0, 1.0, std::abs(relative) * middle - std::abs(direction) + 1.0);
}

float SoulTrainKnob::scale() const noexcept
{
    return std::min(float(getWidth()) / art.width, float(getHeight()) / art.height);
}

juce::AffineTransform SoulTrainKnob::artworkTransform() const noexcept
{
    const auto factor = scale();
    return juce::AffineTransform::scale(factor)
        .translated((float(getWidth()) - art.width * factor) * .5f,
                    (float(getHeight()) - art.height * factor) * .5f);
}

void SoulTrainKnob::drawLayer(juce::Graphics& graphics, const Layer& layer, float opacity) const
{
    graphics.setOpacity(opacity);
    graphics.drawImageTransformed(layer.pixels,
        juce::AffineTransform::scale(1.f / art.pixelScale).translated(layer.origin));
}

void SoulTrainKnob::paint(juce::Graphics& graphics)
{
    juce::Graphics::ScopedSaveState saved(graphics);
    graphics.addTransform(artworkTransform());
    drawLayer(graphics, keyboardFocus ? art.focusedBody : art.body, 1.f);
    const auto relative=position();
   #if JUCE_WINDOWS
    const auto count=art.glow.size();
    size_t first=count,end=0;
    for (size_t index=0;index<count;++index)
        if (glowLevel(index,relative)==1.) {
            first=std::min(first,index);
            end=index+1;
        }
    const auto frame=std::find_if(art.glowFrames.begin(),art.glowFrames.end(),
        [=](const Artwork::GlowFrame& glow){ return glow.first==first && glow.end==end; });
   #endif
    for (size_t index = 0; index < art.glow.size(); ++index)
    {
       #if JUCE_WINDOWS
        if (index==first && frame!=art.glowFrames.end()) {
            drawLayer(graphics,frame->layer,1.f);
            index=end-1;
            continue;
        }
       #endif
        const auto level = float(glowLevel(index,relative));
        if (level > 0)
            drawLayer(graphics, art.glow[index], level);
    }

    graphics.setColour(juce::Colour(0xffd9d9d9));
    graphics.fillPath(marker, juce::AffineTransform::rotation(float(juce::degreesToRadians(angleDegrees())),
                                                             33.5f, 33.5f)
                                  .translated(art.dial.getPosition()));
}

bool SoulTrainKnob::hitTest(int x, int y)
{
    return art.dial.contains(juce::Point<float>(float(x), float(y)).transformedBy(artworkTransform().inverted()));
}

void SoulTrainKnob::mouseDown(const juce::MouseEvent& event)
{
    if (!isEnabled() || !event.mods.isLeftButtonDown()) return;
    keyboardFocus = false;
    repaint();
    dragStartY = event.position.y;
    dragStartValue = getValue();
    if (!drag) drag.emplace(*this);
}

void SoulTrainKnob::mouseDrag(const juce::MouseEvent& event)
{
    if (!drag || !isEnabled() || scale() <= 0) return;
    // Use the same travel for a short parameter range as for a 7-bit one.
    const double rangeScale = juce::jmin(1.0, double(range.maximum - range.minimum) / 127.0);
    const double sensitivity = rangeScale * (event.mods.isShiftDown() ? .12 : .65);
    const double distance = double(dragStartY - event.position.y) / scale();
    setValue(dragStartValue + distance * sensitivity, juce::sendNotificationSync);
}

void SoulTrainKnob::mouseUp(const juce::MouseEvent&)
{
    drag.reset();
}

void SoulTrainKnob::setByUser(double value)
{
    if (!isEnabled() || value == getValue()) return;
    if (drag)
        setValue(value, juce::sendNotificationSync);
    else
    {
        const ScopedDragNotification action(*this);
        setValue(value, juce::sendNotificationSync);
    }
}

void SoulTrainKnob::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (event.mods.isLeftButtonDown())
        setByUser(range.defaultValue);
}

void SoulTrainKnob::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY != 0)
        setByUser(getValue() + (wheel.deltaY > 0 ? 1.0 : -1.0));
}

bool SoulTrainKnob::keyPressed(const juce::KeyPress& key)
{
    const auto step = key.getModifiers().isShiftDown() ? 10.0 : 1.0;
    const auto code = key.getKeyCode();
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey)
        setByUser(getValue() + step);
    else if (code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey)
        setByUser(getValue() - step);
    else if (code == juce::KeyPress::homeKey)
        setByUser(range.minimum);
    else if (code == juce::KeyPress::endKey)
        setByUser(range.maximum);
    else return false;
    return true;
}

void SoulTrainKnob::focusGained(FocusChangeType cause)
{
    keyboardFocus = cause != focusChangedByMouseClick;
    repaint();
}

void SoulTrainKnob::focusLost(FocusChangeType)
{
    keyboardFocus = false;
    repaint();
}
}
