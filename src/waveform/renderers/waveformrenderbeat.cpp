#include "waveform/renderers/waveformrenderbeat.h"

#include <QPainter>

#include "track/track.h"
#include "util/painterscope.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "widget/wskincolor.h"

class QPaintEvent;

namespace {

int firstBeatNumber(const mixxx::Beats& beats) {
    constexpr int kDefaultFirstBeat = 1;
    const QString key = QStringLiteral("first_beat=");
    const QString subVersion = beats.getSubVersion();
    const qsizetype start = subVersion.indexOf(key);
    if (start < 0) {
        return kDefaultFirstBeat;
    }
    bool ok = false;
    const int value = subVersion.mid(start + key.size()).section(';', 0, 0).toInt(&ok);
    return ok && value >= 1 && value <= 4 ? value : kDefaultFirstBeat;
}

} // namespace

WaveformRenderBeat::WaveformRenderBeat(WaveformWidgetRenderer* waveformWidgetRenderer)
        : WaveformRendererAbstract(waveformWidgetRenderer) {
    m_beats.resize(128);
}

WaveformRenderBeat::~WaveformRenderBeat() {
}

void WaveformRenderBeat::setup(const QDomNode& node, const SkinContext& context) {
    m_beatColor = QColor(context.selectString(node, "BeatColor"));
    m_beatColor = WSkinColor::getCorrectColor(m_beatColor).toRgb();
    m_downbeatColor = QColor(context.selectString(node, "DownbeatColor"));
    if (!m_downbeatColor.isValid()) {
        m_downbeatColor = QColor(QStringLiteral("#ff3030"));
    }
    m_downbeatColor = WSkinColor::getCorrectColor(m_downbeatColor).toRgb();
}

void WaveformRenderBeat::draw(QPainter* painter, QPaintEvent* /*event*/) {
    TrackPointer pTrackInfo = m_waveformRenderer->getTrackInfo();

    if (!pTrackInfo) {
        return;
    }

    mixxx::BeatsPointer trackBeats = pTrackInfo->getBeats();
    if (!trackBeats) {
        return;
    }

    int alpha = m_waveformRenderer->getBeatGridAlpha();
    if (alpha == 0) {
        return;
    }
#ifdef MIXXX_USE_QOPENGL
    // Using alpha transparency with drawLines causes a graphical issue when
    // drawing with QPainter on the QOpenGLWindow: instead of individual lines
    // a large rectangle encompassing all beatlines is drawn.
    m_beatColor.setAlphaF(1.f);
    m_downbeatColor.setAlphaF(1.f);
#else
    m_beatColor.setAlphaF(alpha/100.0);
    m_downbeatColor.setAlphaF(alpha / 100.0);
#endif

    const double trackSamples = m_waveformRenderer->getTrackSamples();
    if (trackSamples <= 0) {
        return;
    }

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();

    const double firstDisplayedPosition =
            m_waveformRenderer->getFirstDisplayedPosition();
    const double lastDisplayedPosition =
            m_waveformRenderer->getLastDisplayedPosition();

    // qDebug() << "trackSamples" << trackSamples
    //          << "firstDisplayedPosition" << firstDisplayedPosition
    //          << "lastDisplayedPosition" << lastDisplayedPosition;

    const auto startPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            firstDisplayedPosition * trackSamples);
    const auto endPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            lastDisplayedPosition * trackSamples);
    auto it = trackBeats->iteratorFrom(startPosition);

    // if no beat do not waste time saving/restoring painter
    if (it == trackBeats->cend() || *it > endPosition) {
        return;
    }

    PainterScope PainterScope(painter);

    painter->setRenderHint(QPainter::Antialiasing);

    QPen beatPen(m_beatColor);
    beatPen.setWidthF(std::max(1.0, scaleFactor()));
    painter->setPen(beatPen);

    const Qt::Orientation orientation = m_waveformRenderer->getOrientation();
    const float rendererWidth = m_waveformRenderer->getWidth();
    const float rendererHeight = m_waveformRenderer->getHeight();

    int beatCount = 0;
    int downbeatCount = 0;
    const int firstBeat = firstBeatNumber(*trackBeats);
    const auto firstMarker = trackBeats->cfirstmarker();

    for (; it != trackBeats->cend() && *it <= endPosition; ++it) {
        double beatPosition = it->toEngineSamplePos();
        double xBeatPoint =
                m_waveformRenderer->transformSamplePositionInRendererWorld(beatPosition);

        xBeatPoint = qRound(xBeatPoint * devicePixelRatio) / devicePixelRatio;

        // If we don't have enough space, double the size.
        if (beatCount >= m_beats.size()) {
            m_beats.resize(m_beats.size() * 2);
        }

        const bool isDownbeat = ((it - firstMarker) + firstBeat - 1) % 4 == 0;
        if (isDownbeat && downbeatCount >= m_downbeats.size()) {
            m_downbeats.resize(m_downbeats.isEmpty() ? 128 : m_downbeats.size() * 2);
        }

        if (orientation == Qt::Horizontal) {
            const QLineF line(xBeatPoint, 0.0f, xBeatPoint, rendererHeight);
            m_beats[beatCount++] = line;
            if (isDownbeat) {
                m_downbeats[downbeatCount++] = line;
            }
        } else {
            const QLineF line(0.0f, xBeatPoint, rendererWidth, xBeatPoint);
            m_beats[beatCount++] = line;
            if (isDownbeat) {
                m_downbeats[downbeatCount++] = line;
            }
        }
    }

    // Make sure to use constData to prevent detaches!
    painter->drawLines(m_beats.constData(), beatCount);
    QPen downbeatPen(m_downbeatColor);
    downbeatPen.setWidthF(std::max(2.0, scaleFactor() * 2));
    painter->setPen(downbeatPen);
    painter->drawLines(m_downbeats.constData(), downbeatCount);
}
