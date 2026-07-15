#pragma once

#include "models/Contact.h"

#include <QString>

// Converts between this app's Contact model and vCard text -- the
// interchange format used internally by both KDE's Akonadi/KContacts and
// GNOME's Evolution Data Server (EDS), though neither is wired up as a
// consumer yet (see Task 6+). Pure QString text processing, no platform
// library, no I/O -- stays inside core/'s Qt6::Core/Network/Sql-only
// boundary and has zero dependency on the rest of the native-contacts-sync
// feature.
//
// Targets vCard 3.0 (VERSION:3.0) specifically, not 4.0 -- chosen for more
// consistent support across KContacts::VCardConverter and older EDS builds.
// This is a decision made WITHOUT the ability to spike against real
// KAddressBook/GNOME Contacts exports in this session (no Akonadi/EDS
// installed on this machine) -- vCard-4-specific escaping/structured-value
// differences should be re-checked against real exports before Task 7/8
// ship.
//
// See task-4-brief.md's lossy-mapping decision table for every Contact
// field that doesn't round-trip 1:1 (rev, createdAt, deleted, uid, PO box/
// extended-address, multi-valued middle names, multi-type TYPE= params);
// each decision also has a short comment at its point of implementation in
// VCardContact.cpp.
namespace VCardContact {

QString contactToVCard(const Contact& contact);
Contact contactFromVCard(const QString& vcard);

} // namespace VCardContact
