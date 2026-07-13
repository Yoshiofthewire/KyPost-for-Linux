#include "domain/TransportStateMachine.h"

#include "domain/PushRepository.h"
#include "net/NtfySubscriber.h"

TransportStateMachine::TransportStateMachine(NtfySubscriber& subscriber, PushRepository& pushRepository,
                                               QObject* parent, int pollIntervalMs)
    : QObject(parent)
    , m_subscriber(subscriber)
    , m_pushRepository(pushRepository)
{
    m_pollTimer.setInterval(pollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &TransportStateMachine::onPollTimer);
    connect(&m_subscriber, &NtfySubscriber::connectionLost, this,
            &TransportStateMachine::onSubscriberConnectionLost);
    connect(&m_subscriber, &NtfySubscriber::messageReceived, this, [this](const QJsonObject& data) {
        if (m_tier == TransportTier::EmbeddedSubscriber)
            emit notificationReceived(data);
    });

    // m_tier already defaults to Polling (see header); there is no
    // "previous" tier to leave, so start its timer directly rather than
    // going through enterTier(), which only acts on an actual transition.
    m_pollTimer.start();
}

void TransportStateMachine::setDistributorAvailable(bool available)
{
    m_distributorAvailable = available;
    selectTier();
}

void TransportStateMachine::setForegrounded(bool foregrounded)
{
    m_foregrounded = foregrounded;
    selectTier();
}

TransportTier TransportStateMachine::currentTier() const
{
    return m_tier;
}

void TransportStateMachine::selectTier()
{
    if (m_distributorAvailable)
        enterTier(TransportTier::Distributor);
    else if (m_foregrounded)
        enterTier(TransportTier::EmbeddedSubscriber);
    else
        enterTier(TransportTier::Polling);
}

void TransportStateMachine::onSubscriberConnectionLost(const QString&)
{
    // "subscriber unreachable -> 90s polling" is its own explicit fallback
    // edge, distinct from simply not being foregrounded -- drop straight to
    // Polling without waiting for a setForegrounded/setDistributorAvailable
    // call, and without re-running selectTier() (which would just re-derive
    // the same EmbeddedSubscriber tier from the still-true foreground flag).
    if (m_tier == TransportTier::EmbeddedSubscriber)
        enterTier(TransportTier::Polling);
}

void TransportStateMachine::onPollTimer()
{
    const QVector<PushNotification> delivered = m_pushRepository.pullOnce();
    emit pollTick(delivered);
}

void TransportStateMachine::enterTier(TransportTier tier)
{
    if (tier == m_tier)
        return;
    m_tier = tier;

    if (tier != TransportTier::EmbeddedSubscriber)
        m_subscriber.stop();
    if (tier != TransportTier::Polling)
        m_pollTimer.stop();

    if (tier == TransportTier::EmbeddedSubscriber)
        m_subscriber.start();
    else if (tier == TransportTier::Polling)
        m_pollTimer.start();

    emit tierChanged(tier);
}
