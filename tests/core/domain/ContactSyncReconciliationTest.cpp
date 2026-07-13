#include "domain/ContactSyncReconciliation.h"

#include <QTest>

class ContactSyncReconciliationTest : public QObject
{
    Q_OBJECT

private slots:
    void matchesByContentRegardlessOfOrder();
    void fallsBackToOrderForUnmatchedContent();
    void ignoresDeletedAndUidlessCandidatesAndLeavesExtrasPending();
    void alreadySyncedContactsAreNotReassigned();

private:
    static Contact makeLocal(const QString& uid, const QString& fn, const QString& email);
    static Contact makeResponse(const QString& uid, const QString& fn, const QString& email, bool deleted = false);
};

Contact ContactSyncReconciliationTest::makeLocal(const QString& uid, const QString& fn, const QString& email)
{
    Contact contact;
    contact.uid = uid;
    contact.fn = fn;
    if (!email.isEmpty())
        contact.emails = { ContactEmailEntry{ std::nullopt, email } };
    return contact;
}

Contact ContactSyncReconciliationTest::makeResponse(const QString& uid, const QString& fn, const QString& email,
                                                      bool deleted)
{
    Contact contact;
    contact.uid = uid;
    contact.fn = fn;
    if (!email.isEmpty())
        contact.emails = { ContactEmailEntry{ std::nullopt, email } };
    contact.deleted = deleted;
    return contact;
}

void ContactSyncReconciliationTest::matchesByContentRegardlessOfOrder()
{
    const QVector<Contact> local = {
        makeLocal(QStringLiteral("local-ada"), QStringLiteral("Ada"), QStringLiteral("ada@example.com")),
        makeLocal(QStringLiteral("local-grace"), QStringLiteral("Grace"), QStringLiteral("grace@example.com")),
    };
    const QVector<Contact> response = {
        makeResponse(QStringLiteral("srv-g"), QStringLiteral("Grace"), QStringLiteral("grace@example.com")),
        makeResponse(QStringLiteral("srv-a"), QStringLiteral("Ada"), QStringLiteral("ada@example.com")),
    };

    const QVector<ContactReconciliationAssignment> assignments = ContactSyncReconciliation::reconcile(local, response);

    QCOMPARE(assignments.size(), 2);
    bool foundAda = false;
    bool foundGrace = false;
    for (const ContactReconciliationAssignment& assignment : assignments) {
        if (assignment.localUid == QStringLiteral("local-ada")) {
            QCOMPARE(assignment.serverUid, QStringLiteral("srv-a"));
            foundAda = true;
        } else if (assignment.localUid == QStringLiteral("local-grace")) {
            QCOMPARE(assignment.serverUid, QStringLiteral("srv-g"));
            foundGrace = true;
        }
    }
    QVERIFY(foundAda);
    QVERIFY(foundGrace);
}

void ContactSyncReconciliationTest::fallsBackToOrderForUnmatchedContent()
{
    // Server normalized the names, so content matching fails -- falls back
    // to pairing by order.
    const QVector<Contact> local = {
        makeLocal(QStringLiteral("local-ada"), QStringLiteral("ada lovelace"), QStringLiteral("ada@example.com")),
        makeLocal(QStringLiteral("local-grace"), QStringLiteral("grace hopper"), QStringLiteral("grace@example.com")),
    };
    const QVector<Contact> response = {
        makeResponse(QStringLiteral("srv-1"), QStringLiteral("Ada Lovelace"), QStringLiteral("ada@example.com")),
        makeResponse(QStringLiteral("srv-2"), QStringLiteral("Grace Hopper"), QStringLiteral("grace@example.com")),
    };

    const QVector<ContactReconciliationAssignment> assignments = ContactSyncReconciliation::reconcile(local, response);

    QCOMPARE(assignments.size(), 2);
    QCOMPARE(assignments.at(0).localUid, QStringLiteral("local-ada"));
    QCOMPARE(assignments.at(0).serverUid, QStringLiteral("srv-1"));
    QCOMPARE(assignments.at(1).localUid, QStringLiteral("local-grace"));
    QCOMPARE(assignments.at(1).serverUid, QStringLiteral("srv-2"));
}

void ContactSyncReconciliationTest::ignoresDeletedAndUidlessCandidatesAndLeavesExtrasPending()
{
    const QVector<Contact> local = {
        makeLocal(QStringLiteral("local-ada"), QStringLiteral("Ada"), QStringLiteral("ada@example.com")),
        makeLocal(QStringLiteral("local-grace"), QStringLiteral("Grace"), QStringLiteral("grace@example.com")),
    };
    const QVector<Contact> response = {
        makeResponse(QStringLiteral("srv-x"), QStringLiteral("Someone"), QStringLiteral("x@example.com"), true),
        makeResponse(QString(), QStringLiteral("No uid"), QStringLiteral("nouid@example.com")),
        makeResponse(QStringLiteral("srv-a"), QStringLiteral("Ada"), QStringLiteral("ada@example.com")),
    };

    const QVector<ContactReconciliationAssignment> assignments = ContactSyncReconciliation::reconcile(local, response);

    // Only Ada matches; Grace stays pending for the next sync (the deleted
    // and uid-less candidates are never eligible, so pass 2 has no leftover
    // candidate to pair Grace with).
    QCOMPARE(assignments.size(), 1);
    QCOMPARE(assignments.at(0).localUid, QStringLiteral("local-ada"));
    QCOMPARE(assignments.at(0).serverUid, QStringLiteral("srv-a"));
}

void ContactSyncReconciliationTest::alreadySyncedContactsAreNotReassigned()
{
    // localPending only ever contains queued creates; a contact that
    // already has a real server uid has nothing to reconcile.
    const QVector<Contact> local = {};
    const QVector<Contact> response = {
        makeResponse(QStringLiteral("srv-new"), QStringLiteral("Ada"), QStringLiteral("ada@example.com")),
    };

    const QVector<ContactReconciliationAssignment> assignments = ContactSyncReconciliation::reconcile(local, response);

    QVERIFY(assignments.isEmpty());
}

QTEST_APPLESS_MAIN(ContactSyncReconciliationTest)
#include "ContactSyncReconciliationTest.moc"
