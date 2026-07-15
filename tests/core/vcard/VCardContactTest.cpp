#include "vcard/VCardContact.h"

#include "models/Contact.h"

#include <QTest>

class VCardContactTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsEveryScalarField();
    void roundTripsEmailsPhonesAddressesWithLabels();
    void versionLineIsAlwaysThreeDotZero();
    void neverWritesRevCreatedAtOrDeleted();
    void parsedUidNeverPopulatesContactUid();
    void updatedAtRoundTripsViaRevAndIsNulloptWhenAbsent();
    void multiValuedAdditionalNameJoinsWithSpaceOnRead();
    void singleMiddleNameRoundTripsAsSingleNComponent();
    void emailTypeRoundTripsFirstNonPrefTokenLowercased();
    void multiTypeCollapsesToFirstToken();
    void addressPoBoxAndExtendedAlwaysEmpty();
    void longNoteFoldsAndUnfoldsExactly();
    void noteEscapesCommaSemicolonBackslashAndNewline();
};

void VCardContactTest::roundTripsEveryScalarField()
{
    Contact contact;
    contact.fn = QStringLiteral("Ada Lovelace");
    contact.givenName = QStringLiteral("Ada");
    contact.familyName = QStringLiteral("Lovelace");
    contact.middleName = QStringLiteral("Augusta");
    contact.prefix = QStringLiteral("Countess");
    contact.suffix = QStringLiteral("Esq.");
    contact.nickname = QStringLiteral("Ada");
    contact.org = QStringLiteral("Analytical Engines Ltd");
    contact.title = QStringLiteral("Mathematician");
    contact.notes = QStringLiteral("Pioneer of computing");
    contact.birthday = QStringLiteral("1815-12-10");

    const QString vcard = VCardContact::contactToVCard(contact);
    const Contact roundTripped = VCardContact::contactFromVCard(vcard);

    QCOMPARE(roundTripped.fn, contact.fn);
    QCOMPARE(roundTripped.givenName, contact.givenName);
    QCOMPARE(roundTripped.familyName, contact.familyName);
    QCOMPARE(roundTripped.middleName, contact.middleName);
    QCOMPARE(roundTripped.prefix, contact.prefix);
    QCOMPARE(roundTripped.suffix, contact.suffix);
    QCOMPARE(roundTripped.nickname, contact.nickname);
    QCOMPARE(roundTripped.org, contact.org);
    QCOMPARE(roundTripped.title, contact.title);
    QCOMPARE(roundTripped.notes, contact.notes);
    QCOMPARE(roundTripped.birthday, contact.birthday);
}

void VCardContactTest::roundTripsEmailsPhonesAddressesWithLabels()
{
    Contact contact;
    contact.emails = { ContactEmailEntry{ QStringLiteral("work"), QStringLiteral("ada@example.com") },
                       ContactEmailEntry{ std::nullopt, QStringLiteral("nolabel@example.com") } };
    contact.phones = { ContactPhoneEntry{ QStringLiteral("mobile"), QStringLiteral("+1-555-0100") } };
    contact.addresses = { ContactAddressEntry{ QStringLiteral("home"), QStringLiteral("1 Main St"),
                                                QStringLiteral("London"), QStringLiteral("London"),
                                                QStringLiteral("SW1A 1AA"), QStringLiteral("UK") } };

    const QString vcard = VCardContact::contactToVCard(contact);
    const Contact roundTripped = VCardContact::contactFromVCard(vcard);

    QCOMPARE(roundTripped.emails, contact.emails);
    QCOMPARE(roundTripped.phones, contact.phones);
    QCOMPARE(roundTripped.addresses, contact.addresses);
}

void VCardContactTest::versionLineIsAlwaysThreeDotZero()
{
    Contact contact;
    contact.fn = QStringLiteral("Plain");
    const QString vcard = VCardContact::contactToVCard(contact);
    QVERIFY(vcard.contains(QStringLiteral("VERSION:3.0")));
}

void VCardContactTest::neverWritesRevCreatedAtOrDeleted()
{
    Contact contact;
    contact.uid = QStringLiteral("relay-uid-99");
    contact.rev = 42;
    contact.createdAt = QStringLiteral("2020-01-01T00:00:00Z");
    contact.deleted = true;
    contact.fn = QStringLiteral("Someone");

    const QString vcard = VCardContact::contactToVCard(contact);

    QVERIFY(!vcard.contains(QStringLiteral("42")));
    QVERIFY(!vcard.contains(QStringLiteral("2020-01-01T00:00:00Z")));
    QVERIFY(!vcard.contains(QStringLiteral("relay-uid-99")));
    QVERIFY(!vcard.contains(QStringLiteral("UID:")));
    QVERIFY(!vcard.contains(QStringLiteral("DELETED"), Qt::CaseInsensitive));

    // On read there's nothing in the text to recover these from either --
    // createdAt stays nullopt (caller-side merge fallback) and deleted
    // stays false (tombstones live at the provider-adapter level, not here).
    const Contact roundTripped = VCardContact::contactFromVCard(vcard);
    QCOMPARE(roundTripped.createdAt, std::optional<QString>(std::nullopt));
    QCOMPARE(roundTripped.deleted, false);
    QVERIFY(roundTripped.uid.isEmpty());
}

void VCardContactTest::parsedUidNeverPopulatesContactUid()
{
    const QString vcard = QStringLiteral("BEGIN:VCARD\r\n"
                                          "VERSION:3.0\r\n"
                                          "UID:native-item-12345\r\n"
                                          "FN:Someone\r\n"
                                          "END:VCARD\r\n");

    const Contact contact = VCardContact::contactFromVCard(vcard);

    QVERIFY(contact.uid.isEmpty());
    QCOMPARE(contact.fn, std::optional<QString>(QStringLiteral("Someone")));
}

void VCardContactTest::updatedAtRoundTripsViaRevAndIsNulloptWhenAbsent()
{
    Contact withUpdatedAt;
    withUpdatedAt.updatedAt = QStringLiteral("2026-02-01T00:00:00Z");
    const QString vcard = VCardContact::contactToVCard(withUpdatedAt);
    QVERIFY(vcard.contains(QStringLiteral("REV:2026-02-01T00:00:00Z")));
    const Contact roundTripped = VCardContact::contactFromVCard(vcard);
    QCOMPARE(roundTripped.updatedAt, withUpdatedAt.updatedAt);

    Contact withoutUpdatedAt;
    const QString vcard2 = VCardContact::contactToVCard(withoutUpdatedAt);
    QVERIFY(!vcard2.contains(QStringLiteral("REV:")));
    const Contact roundTripped2 = VCardContact::contactFromVCard(vcard2);
    QCOMPARE(roundTripped2.updatedAt, std::optional<QString>(std::nullopt));
}

void VCardContactTest::multiValuedAdditionalNameJoinsWithSpaceOnRead()
{
    const QString vcard = QStringLiteral("BEGIN:VCARD\r\n"
                                          "VERSION:3.0\r\n"
                                          "N:Stevenson;John;Philip,Paul;Dr.;Jr.\r\n"
                                          "END:VCARD\r\n");

    const Contact contact = VCardContact::contactFromVCard(vcard);
    QCOMPARE(contact.middleName, std::optional<QString>(QStringLiteral("Philip Paul")));
    QCOMPARE(contact.familyName, std::optional<QString>(QStringLiteral("Stevenson")));
    QCOMPARE(contact.givenName, std::optional<QString>(QStringLiteral("John")));
    QCOMPARE(contact.prefix, std::optional<QString>(QStringLiteral("Dr.")));
    QCOMPARE(contact.suffix, std::optional<QString>(QStringLiteral("Jr.")));
}

void VCardContactTest::singleMiddleNameRoundTripsAsSingleNComponent()
{
    Contact contact;
    contact.middleName = QStringLiteral("Augusta");
    const QString vcard = VCardContact::contactToVCard(contact);
    QVERIFY(vcard.contains(QStringLiteral("N:;;Augusta;;")));

    const Contact roundTripped = VCardContact::contactFromVCard(vcard);
    QCOMPARE(roundTripped.middleName, contact.middleName);
}

void VCardContactTest::emailTypeRoundTripsFirstNonPrefTokenLowercased()
{
    Contact contact;
    contact.emails = { ContactEmailEntry{ QStringLiteral("work"), QStringLiteral("a@example.com") } };
    const QString vcard = VCardContact::contactToVCard(contact);
    QVERIFY(vcard.contains(QStringLiteral("EMAIL;TYPE=WORK:a@example.com")));

    const Contact roundTripped = VCardContact::contactFromVCard(vcard);
    QCOMPARE(roundTripped.emails.first().label, std::optional<QString>(QStringLiteral("work")));
}

void VCardContactTest::multiTypeCollapsesToFirstToken()
{
    const QString vcard = QStringLiteral("BEGIN:VCARD\r\n"
                                          "VERSION:3.0\r\n"
                                          "EMAIL;TYPE=PREF,HOME,VOICE:a@example.com\r\n"
                                          "TEL;TYPE=HOME,VOICE:+15550100\r\n"
                                          "END:VCARD\r\n");

    const Contact contact = VCardContact::contactFromVCard(vcard);
    QCOMPARE(contact.emails.size(), 1);
    QCOMPARE(contact.emails.first().label, std::optional<QString>(QStringLiteral("home")));
    QCOMPARE(contact.phones.size(), 1);
    QCOMPARE(contact.phones.first().label, std::optional<QString>(QStringLiteral("home")));
}

void VCardContactTest::addressPoBoxAndExtendedAlwaysEmpty()
{
    Contact contact;
    contact.addresses = { ContactAddressEntry{ std::nullopt, QStringLiteral("1 Main St"), QStringLiteral("London"),
                                                std::nullopt, std::nullopt, std::nullopt } };
    const QString vcard = VCardContact::contactToVCard(contact);
    QVERIFY(vcard.contains(QStringLiteral("ADR:;;1 Main St;London;;;")));

    // A hand-crafted vCard with non-empty PO box/extended-address components
    // -- both must be ignored on read (Contact has no fields for them).
    const QString withPoBox = QStringLiteral("BEGIN:VCARD\r\n"
                                              "VERSION:3.0\r\n"
                                              "ADR:PO Box 42;Suite 9;1 Main St;London;;;\r\n"
                                              "END:VCARD\r\n");
    const Contact parsed = VCardContact::contactFromVCard(withPoBox);
    QCOMPARE(parsed.addresses.size(), 1);
    QCOMPARE(parsed.addresses.first().street, std::optional<QString>(QStringLiteral("1 Main St")));
    QCOMPARE(parsed.addresses.first().city, std::optional<QString>(QStringLiteral("London")));
}

void VCardContactTest::longNoteFoldsAndUnfoldsExactly()
{
    const QString longNote = QStringLiteral(
        "This is a deliberately long note field that must exceed the 75-octet "
        "RFC 6350 line-folding threshold so we can verify both that "
        "contactToVCard actually folds it into multiple physical lines and "
        "that contactFromVCard correctly unfolds it back to this exact string, "
        "with no characters lost, duplicated, or reordered along the way.");
    QVERIFY(longNote.toUtf8().size() > 75);

    Contact contact;
    contact.notes = longNote;
    const QString vcard = VCardContact::contactToVCard(contact);

    // Folding evidence: at least one CRLF-plus-leading-space continuation
    // present, and no physical line's UTF-8 byte length exceeds 75 octets.
    QVERIFY(vcard.contains(QStringLiteral("\r\n ")));
    const QStringList physicalLines = vcard.split(QStringLiteral("\r\n"));
    for (const QString& line : physicalLines) {
        if (line.isEmpty())
            continue;
        QVERIFY(line.toUtf8().size() <= 75);
    }

    const Contact roundTripped = VCardContact::contactFromVCard(vcard);
    QCOMPARE(roundTripped.notes, contact.notes);
}

void VCardContactTest::noteEscapesCommaSemicolonBackslashAndNewline()
{
    const QString tricky =
        QStringLiteral("Line one, with a comma; a semicolon; a backslash \\ and\nan embedded newline.");
    Contact contact;
    contact.notes = tricky;
    const QString vcard = VCardContact::contactToVCard(contact);

    // Escaped on write: reserved characters must appear in escaped form
    // somewhere in the generated text.
    QVERIFY(vcard.contains(QStringLiteral("\\,")));
    QVERIFY(vcard.contains(QStringLiteral("\\;")));
    QVERIFY(vcard.contains(QStringLiteral("\\\\")));
    QVERIFY(vcard.contains(QStringLiteral("\\n")));

    const Contact roundTripped = VCardContact::contactFromVCard(vcard);
    QCOMPARE(roundTripped.notes, contact.notes);
}

QTEST_GUILESS_MAIN(VCardContactTest)
#include "VCardContactTest.moc"
