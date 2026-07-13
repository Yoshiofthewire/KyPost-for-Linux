#pragma once

#include <QString>
#include <QVector>
#include <optional>

struct ContactEmailEntry
{
    std::optional<QString> label;
    QString value;

    bool operator==(const ContactEmailEntry&) const = default;
};

struct ContactPhoneEntry
{
    std::optional<QString> label;
    QString value;

    bool operator==(const ContactPhoneEntry&) const = default;
};

struct ContactAddressEntry
{
    std::optional<QString> label;
    std::optional<QString> street;
    std::optional<QString> city;
    std::optional<QString> region;
    std::optional<QString> postalCode;
    std::optional<QString> country;

    bool operator==(const ContactAddressEntry&) const = default;
};

struct Contact
{
    QString uid;
    qint64 rev = 0;
    std::optional<QString> createdAt;
    std::optional<QString> updatedAt;
    std::optional<QString> fn;
    std::optional<QString> givenName;
    std::optional<QString> familyName;
    std::optional<QString> middleName;
    std::optional<QString> prefix;
    std::optional<QString> suffix;
    std::optional<QString> nickname;
    std::optional<QString> org;
    std::optional<QString> title;
    std::optional<QString> notes;
    std::optional<QString> birthday;
    QVector<ContactEmailEntry> emails;
    QVector<ContactPhoneEntry> phones;
    QVector<ContactAddressEntry> addresses;
    bool deleted = false; // wire "deleted" (json:"deleted,omitempty" in the Go
                           // Contact struct) -- a push entry with {uid, rev,
                           // deleted:true} tombstones server-side, every
                           // other field omitted/zero.

    bool operator==(const Contact&) const = default;
};
