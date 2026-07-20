#pragma once

#include "db/PushDao.h"
#include "models/PushNotification.h"

#include <QString>
#include <QVector>

class CursorStore;
class PairingStore;
class PushNotificationClient;
class SettingsStore;

// Sits between PushNotificationClient and PushDao, matching the
// Domain/Repositories layer in kypost-for-Mac (PushRepository.swift) --
// port of PushRepositoryTests from PushTests.swift, with one deliberate
// deviation documented on recordPushArrival() below.
class PushRepository
{
public:
    PushRepository(PushDao& pushDao, CursorStore& cursorStore, PushNotificationClient& client,
                    PairingStore& pairingStore, SettingsStore& settingsStore);

    // Most-recent-first, up to `limit`.
    QVector<PushRecord> history(int limit = 50) const;

    void markRead(const QString& messageId); // pushDao.markConsumed

    // Records a push-mode arrival (no server seq on this path -- one is
    // synthesized from arrival time). Returns the record actually
    // persisted (its .seq may differ from the naive receivedAtEpochMs on
    // a collision).
    //
    // Deviation from PushTests.swift's pushArrivalsGetUniqueSynthesizedSeqs,
    // deliberate, not an oversight: that test asserts the *same* messageId
    // arriving twice at the same instant produces two separate history
    // rows with different seqs. Our push_notifications table is keyed by
    // message_id (a Phase 2 decision, already shipped) -- insertOrReplace
    // on a repeat messageId overwrites the existing row rather than adding
    // a second one, which is the intentionally correct behavior for our
    // schema (two notifications about the *same* message shouldn't produce
    // two history entries). See PushRepositoryTest for the two assertions
    // that replace the Swift test's single assertion.
    PushRecord recordPushArrival(const PushNotification& payload, qint64 receivedAtEpochMs);

    // One poll of the pull endpoint. Returns newly-delivered notifications
    // (already persisted). NotPaired-equivalent: returns an empty vector
    // when there is no stored pairing -- this method has no error-outcome
    // return type (unlike the repositories above) because polling is
    // expected to run silently and retry on its own schedule (Task 25
    // owns the retry/backoff policy); a caller that needs to distinguish
    // "no pairing" from "polled, nothing new" should check
    // PairingStore::isPaired() itself first.
    QVector<PushNotification> pullOnce();

private:
    QString resolvePullEndpoint(const QString& serverBaseUrl) const;

    PushDao& m_pushDao;
    CursorStore& m_cursorStore;
    PushNotificationClient& m_client;
    PairingStore& m_pairingStore;
    SettingsStore& m_settingsStore;
};
