/*
 * This file is part of TelepathyQt4
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <TelepathyQt4/Client/ContactManager>
#include "TelepathyQt4/Client/_gen/contact-manager.moc.hpp"

#include <QMap>
#include <QString>
#include <QSet>
#include <QWeakPointer>

#include <TelepathyQt4/Client/Connection>
#include <TelepathyQt4/Client/PendingContactAttributes>
#include <TelepathyQt4/Client/PendingContacts>
#include <TelepathyQt4/Client/PendingFailure>
#include <TelepathyQt4/Client/PendingHandles>
#include <TelepathyQt4/Client/ReferencedHandles>

#include "TelepathyQt4/debug-internal.h"

/**
 * \addtogroup clientsideproxies Client-side proxies
 *
 * Proxy objects representing remote service objects accessed via D-Bus.
 *
 * In addition to providing direct access to methods, signals and properties
 * exported by the remote objects, some of these proxies offer features like
 * automatic inspection of remote object capabilities, property tracking,
 * backwards compatibility helpers for older services and other utilities.
 */

/**
 * \defgroup clientconn Connection proxies
 * \ingroup clientsideproxies
 *
 * Proxy objects representing remote Telepathy Connection objects.
 */

namespace Telepathy
{
namespace Client
{

/**
 * \class ContactManager
 * \ingroup clientconn
 * \headerfile <TelepathyQt4/Client/contact-manager.h> <TelepathyQt4/Client/ContactManager>
 */

struct ContactManager::Private
{
    Connection *conn;
    QMap<uint, QWeakPointer<Contact> > contacts;

    QMap<Contact::Feature, bool> tracking;
    void ensureTracking(Contact::Feature feature);

    QSet<Contact::Feature> supportedFeatures;

    QMap<uint, ContactListChannel> contactListsChannels;
    QSet<QSharedPointer<Contact> > allKnownContacts() const;
    void updateContactsPresenceState();
};

Connection *ContactManager::connection() const
{
    return mPriv->conn;
}

bool ContactManager::isSupported() const
{
    if (!connection()->isReady()) {
        warning() << "ContactManager::isSupported() used before the connection is ready!";
        return false;
    } /* FIXME: readd this check when Connection is no longer a steaming pile of junk: else if (connection()->status() != Connection::StatusConnected) {
        warning() << "ContactManager::isSupported() used before the connection is connected!";
        return false;
    } */

    return connection()->interfaces().contains(TELEPATHY_INTERFACE_CONNECTION_INTERFACE_CONTACTS);
}

namespace
{
QString featureToInterface(Contact::Feature feature)
{
    switch (feature) {
        case Contact::FeatureAlias:
            return TELEPATHY_INTERFACE_CONNECTION_INTERFACE_ALIASING;
        case Contact::FeatureAvatarToken:
            return TELEPATHY_INTERFACE_CONNECTION_INTERFACE_AVATARS;
        case Contact::FeatureSimplePresence:
            return TELEPATHY_INTERFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE;
        default:
            warning() << "ContactManager doesn't know which interface corresponds to feature"
                << feature;
            return QString();
    }
}
}

QSet<Contact::Feature> ContactManager::supportedFeatures() const
{
    if (!isSupported()) {
        warning() << "ContactManager::supportedFeatures() used with the entire ContactManager"
            << "functionality being unsupported, returning an empty set";
        return QSet<Contact::Feature>();
    }

    if (mPriv->supportedFeatures.isEmpty()) {
        QList<Contact::Feature> allFeatures = QList<Contact::Feature>()
            << Contact::FeatureAlias
            << Contact::FeatureAvatarToken
            << Contact::FeatureSimplePresence;
        QStringList interfaces = mPriv->conn->contactAttributeInterfaces();

        foreach (Contact::Feature feature, allFeatures) {
            if (interfaces.contains(featureToInterface(feature))) {
                mPriv->supportedFeatures.insert(feature);
            }
        }

        debug() << mPriv->supportedFeatures.size() << "contact features supported using" << this;
    }

    return mPriv->supportedFeatures;
}

PendingContacts *ContactManager::allKnownContacts(
        const QSet<Contact::Feature> &features)
{
    return upgradeContacts(mPriv->allKnownContacts().toList(), features);
}

bool ContactManager::canRequestContactsPresenceSubscription() const
{
    QSharedPointer<Channel> subscribeChannel;
    if (mPriv->contactListsChannels.contains(ContactListChannel::TypeSubscribe)) {
        subscribeChannel = mPriv->contactListsChannels[ContactListChannel::TypeSubscribe].channel;
    }
    return subscribeChannel && subscribeChannel->groupCanAddContacts();
}

PendingOperation *ContactManager::requestContactsPresenceSubscription(
        const QList<QSharedPointer<Contact> > &contacts, const QString &message)
{
    if (!canRequestContactsPresenceSubscription()) {
        return new PendingFailure(this, TELEPATHY_ERROR_NOT_IMPLEMENTED,
                "Cannot request contacts presence subscription");
    }

    QSharedPointer<Channel> subscribeChannel =
        mPriv->contactListsChannels[ContactListChannel::TypeSubscribe].channel;
    return subscribeChannel->groupAddContacts(contacts);
}

bool ContactManager::canAuthorizeContactsPresencePublication() const
{
    QSharedPointer<Channel> publishChannel;
    if (mPriv->contactListsChannels.contains(ContactListChannel::TypePublish)) {
        publishChannel = mPriv->contactListsChannels[ContactListChannel::TypePublish].channel;
    }
    return publishChannel && publishChannel->groupCanAddContacts();
}

PendingOperation *ContactManager::authorizeContactsPresencePublication(
        const QList<QSharedPointer<Contact> > &contacts, const QString &message)
{
    if (!canAuthorizeContactsPresencePublication()) {
        return new PendingFailure(this, TELEPATHY_ERROR_NOT_IMPLEMENTED,
                "Cannot authorize contacts presence publication");
    }

    QSharedPointer<Channel> publishChannel =
        mPriv->contactListsChannels[ContactListChannel::TypePublish].channel;
    return publishChannel->groupAddContacts(contacts);
}

bool ContactManager::canDenyContactsPresencePublication() const
{
    QSharedPointer<Channel> publishChannel;
    if (mPriv->contactListsChannels.contains(ContactListChannel::TypePublish)) {
        publishChannel = mPriv->contactListsChannels[ContactListChannel::TypePublish].channel;
    }
    return publishChannel && publishChannel->groupCanRescindContacts();
}

PendingOperation *ContactManager::denyContactsPresencePublication(
        const QList<QSharedPointer<Contact> > &contacts, const QString &message)
{
    if (!canDenyContactsPresencePublication()) {
        return new PendingFailure(this, TELEPATHY_ERROR_NOT_IMPLEMENTED,
                "Cannot deny contacts presence publication");
    }

    QSharedPointer<Channel> publishChannel =
        mPriv->contactListsChannels[ContactListChannel::TypePublish].channel;
    return publishChannel->groupRemoveContacts(contacts);
}

PendingContacts *ContactManager::contactsForHandles(const UIntList &handles,
        const QSet<Contact::Feature> &features)
{
    debug() << "Building contacts for" << handles.size() << "handles" << "with" << features.size()
        << "features";

    QMap<uint, QSharedPointer<Contact> > satisfyingContacts;
    QSet<uint> otherContacts;
    QSet<Contact::Feature> missingFeatures;

    foreach (uint handle, handles) {
        QSharedPointer<Contact> contact = lookupContactByHandle(handle);
        if (contact) {
            if ((features - contact->requestedFeatures()).isEmpty()) {
                // Contact exists and has all the requested features
                satisfyingContacts.insert(handle, contact);
            } else {
                // Contact exists but is missing features
                otherContacts.insert(handle);
                missingFeatures.unite(features - contact->requestedFeatures());
            }
        } else {
            // Contact doesn't exist - we need to get all of the features (same as unite(features))
            missingFeatures = features;
            otherContacts.insert(handle);
        }
    }

    debug() << " " << satisfyingContacts.size() << "satisfying and"
                   << otherContacts.size() << "other contacts";
    debug() << " " << missingFeatures.size() << "features missing";

    QSet<Contact::Feature> supported = supportedFeatures();
    QSet<QString> interfaces;
    foreach (Contact::Feature feature, missingFeatures) {
        mPriv->ensureTracking(feature);

        if (supported.contains(feature)) {
            // Only query interfaces which are reported as supported to not get an error
            interfaces.insert(featureToInterface(feature));
        }
    }

    PendingContacts *contacts =
        new PendingContacts(this, handles, features, satisfyingContacts);

    if (!otherContacts.isEmpty()) {
        debug() << " Fetching" << interfaces.size() << "interfaces for"
                               << otherContacts.size() << "contacts";

        PendingContactAttributes *attributes =
            mPriv->conn->getContactAttributes(otherContacts.toList(), interfaces.toList(), true);

        contacts->connect(attributes,
                SIGNAL(finished(Telepathy::Client::PendingOperation*)),
                SLOT(onAttributesFinished(Telepathy::Client::PendingOperation*)));
    } else {
        contacts->allAttributesFetched();
    }

    return contacts;
}

PendingContacts *ContactManager::contactsForHandles(const ReferencedHandles &handles,
        const QSet<Contact::Feature> &features)
{
    return contactsForHandles(handles.toList(), features);
}

PendingContacts *ContactManager::contactsForIdentifiers(const QStringList &identifiers,
        const QSet<Contact::Feature> &features)
{
    debug() << "Building contacts for" << identifiers.size() << "identifiers" << "with" << features.size()
        << "features";

    PendingHandles *handles = mPriv->conn->requestHandles(HandleTypeContact, identifiers);

    PendingContacts *contacts = new PendingContacts(this, identifiers, features);
    contacts->connect(handles,
            SIGNAL(finished(Telepathy::Client::PendingOperation*)),
            SLOT(onHandlesFinished(Telepathy::Client::PendingOperation*)));

    return contacts;
}

PendingContacts *ContactManager::upgradeContacts(const QList<QSharedPointer<Contact> > &contacts,
        const QSet<Contact::Feature> &features)
{
    debug() << "Upgrading" << contacts.size() << "contacts to have at least"
                           << features.size() << "features";

    return new PendingContacts(this, contacts, features);
}

void ContactManager::onAliasesChanged(const Telepathy::AliasPairList &aliases)
{
    debug() << "Got AliasesChanged for" << aliases.size() << "contacts";

    foreach (AliasPair pair, aliases) {
        QSharedPointer<Contact> contact = lookupContactByHandle(pair.handle);

        if (contact) {
            contact->receiveAlias(pair.alias);
        }
    }
}

void ContactManager::onAvatarUpdated(uint handle, const QString &token)
{
    debug() << "Got AvatarUpdate for contact with handle" << handle;

    QSharedPointer<Contact> contact = lookupContactByHandle(handle);
    if (contact) {
        contact->receiveAvatarToken(token);
    }
}

void ContactManager::onPresencesChanged(const Telepathy::SimpleContactPresences &presences)
{
    debug() << "Got PresencesChanged for" << presences.size() << "contacts";

    foreach (uint handle, presences.keys()) {
        QSharedPointer<Contact> contact = lookupContactByHandle(handle);

        if (contact) {
            contact->receiveSimplePresence(presences[handle]);
        }
    }
}

void ContactManager::onSubscribeChannelMembersChanged(
        const QSet<QSharedPointer<Contact> > &groupMembersAdded,
        const QSet<QSharedPointer<Contact> > &groupLocalPendingMembersAdded,
        const QSet<QSharedPointer<Contact> > &groupRemotePendingMembersAdded,
        const QSet<QSharedPointer<Contact> > &groupMembersRemoved,
        const Channel::GroupMemberChangeDetails &details)
{
    Q_UNUSED(groupRemotePendingMembersAdded);
    Q_UNUSED(details);

    foreach (QSharedPointer<Contact> contact, groupMembersAdded) {
        contact->setSubscriptionState(Contact::PresenceStateYes);
    }

    foreach (QSharedPointer<Contact> contact, groupLocalPendingMembersAdded) {
        contact->setSubscriptionState(Contact::PresenceStateYes);
    }

    foreach (QSharedPointer<Contact> contact, groupMembersRemoved) {
        contact->setSubscriptionState(Contact::PresenceStateNo);
    }
}

void ContactManager::onPublishChannelMembersChanged(
        const QSet<QSharedPointer<Contact> > &groupMembersAdded,
        const QSet<QSharedPointer<Contact> > &groupLocalPendingMembersAdded,
        const QSet<QSharedPointer<Contact> > &groupRemotePendingMembersAdded,
        const QSet<QSharedPointer<Contact> > &groupMembersRemoved,
        const Channel::GroupMemberChangeDetails &details)
{
    Q_UNUSED(groupLocalPendingMembersAdded);
    Q_UNUSED(details);

    foreach (QSharedPointer<Contact> contact, groupMembersAdded) {
        contact->setPublishState(Contact::PresenceStateYes);
    }

    foreach (QSharedPointer<Contact> contact, groupRemotePendingMembersAdded) {
        contact->setPublishState(Contact::PresenceStateYes);
    }

    foreach (QSharedPointer<Contact> contact, groupMembersRemoved) {
        contact->setPublishState(Contact::PresenceStateNo);
    }

    if (!groupRemotePendingMembersAdded.isEmpty()) {
        emit presencePublicationRequested(groupRemotePendingMembersAdded);
    }
}

ContactManager::ContactManager(Connection *parent)
    : QObject(parent), mPriv(new Private)
{
    mPriv->conn = parent;
}

ContactManager::~ContactManager()
{
    delete mPriv;
}

QSharedPointer<Contact> ContactManager::ensureContact(const ReferencedHandles &handle,
        const QSet<Contact::Feature> &features, const QVariantMap &attributes) {
    uint bareHandle = handle[0];
    QSharedPointer<Contact> contact = lookupContactByHandle(bareHandle);

    if (!contact) {
        contact = QSharedPointer<Contact>(new Contact(this, handle, features, attributes));
        mPriv->contacts.insert(bareHandle, contact);
    } else {
        contact->augment(features, attributes);
    }

    return contact;
}

void ContactManager::setContactListChannels(
        const QMap<uint, ContactListChannel> &contactListsChannels)
{
    Q_ASSERT(mPriv->contactListsChannels.isEmpty());
    mPriv->contactListsChannels = contactListsChannels;

    mPriv->updateContactsPresenceState();

    QMap<uint, ContactListChannel>::const_iterator i = contactListsChannels.constBegin();
    QMap<uint, ContactListChannel>::const_iterator end = contactListsChannels.constEnd();
    uint type;
    QSharedPointer<Channel> channel;
    const char *method;
    while (i != end) {
        type = i.key();
        channel = i.value().channel;
        if (!channel) {
            continue;
        }

        if (type == ContactListChannel::TypeSubscribe) {
            method = SLOT(onSubscribeChannelMembersChanged(
                        const QSet<QSharedPointer<Contact> > &,
                        const QSet<QSharedPointer<Contact> > &,
                        const QSet<QSharedPointer<Contact> > &,
                        const QSet<QSharedPointer<Contact> > &,
                        const Channel::GroupMemberChangeDetails &));
        } else if (type == ContactListChannel::TypePublish) {
            method = SLOT(onPublishChannelMembersChanged(
                        const QSet<QSharedPointer<Contact> > &,
                        const QSet<QSharedPointer<Contact> > &,
                        const QSet<QSharedPointer<Contact> > &,
                        const QSet<QSharedPointer<Contact> > &,
                        const Channel::GroupMemberChangeDetails &));
        } else {
            continue;
        }

        connect(channel.data(),
                SIGNAL(groupMembersChanged(
                        const QSet<QSharedPointer<Contact> > &,
                        const QSet<QSharedPointer<Contact> > &,
                        const QSet<QSharedPointer<Contact> > &,
                        const QSet<QSharedPointer<Contact> > &,
                        const Channel::GroupMemberChangeDetails &)),
                method);

        ++i;
    }
}

QSharedPointer<Contact> ContactManager::lookupContactByHandle(uint handle)
{
    QSharedPointer<Contact> contact;

    if (mPriv->contacts.contains(handle)) {
        contact = mPriv->contacts.value(handle).toStrongRef();
        if (!contact) {
            // Dangling weak pointer, remove it
            mPriv->contacts.remove(handle);
        }
    }

    return contact;
}

void ContactManager::Private::ensureTracking(Contact::Feature feature)
{
    if (tracking[feature])
        return;

    switch (feature) {
        case Contact::FeatureAlias:
            QObject::connect(
                    conn->aliasingInterface(),
                    SIGNAL(AliasesChanged(const Telepathy::AliasPairList &)),
                    conn->contactManager(),
                    SLOT(onAliasesChanged(const Telepathy::AliasPairList &)));
            break;

        case Contact::FeatureAvatarToken:
            QObject::connect(
                    conn->avatarsInterface(),
                    SIGNAL(AvatarUpdated(uint, const QString &)),
                    conn->contactManager(),
                    SLOT(onAvatarUpdated(uint, const QString &)));
            break;

        case Contact::FeatureSimplePresence:
            QObject::connect(
                    conn->simplePresenceInterface(),
                    SIGNAL(PresencesChanged(const Telepathy::SimpleContactPresences &)),
                    conn->contactManager(),
                    SLOT(onPresencesChanged(const Telepathy::SimpleContactPresences &)));
            break;

        default:
            warning() << " Unknown feature" << feature
                << "when trying to figure out how to connect change notification!";
            return;
    }

    tracking[feature] = true;
}

QSet<QSharedPointer<Contact> > ContactManager::Private::allKnownContacts() const
{
    QSet<QSharedPointer<Contact> > contacts;
    foreach (const ContactListChannel &contactListChannel, contactListsChannels) {
        QSharedPointer<Channel> channel = contactListChannel.channel;
        if (!channel) {
            continue;
        }
        contacts.unite(channel->groupContacts());
        contacts.unite(channel->groupLocalPendingContacts());
        contacts.unite(channel->groupRemotePendingContacts());
    }
    return contacts;
}

void ContactManager::Private::updateContactsPresenceState()
{
    QSharedPointer<Channel> subscribeChannel;
    QSet<QSharedPointer<Contact> > subscribeContacts;
    QSet<QSharedPointer<Contact> > subscribeContactsLP;
    if (contactListsChannels.contains(ContactListChannel::TypeSubscribe)) {
        subscribeChannel = contactListsChannels[ContactListChannel::TypeSubscribe].channel;
        if (subscribeChannel) {
            subscribeContacts = subscribeChannel->groupContacts();
            subscribeContactsLP = subscribeChannel->groupLocalPendingContacts();
        }
    }

    QSharedPointer<Channel> publishChannel;
    QSet<QSharedPointer<Contact> > publishContacts;
    QSet<QSharedPointer<Contact> > publishContactsRP;
    if (contactListsChannels.contains(ContactListChannel::TypePublish)) {
        publishChannel = contactListsChannels[ContactListChannel::TypePublish].channel;
        if (publishChannel) {
            publishContacts = publishChannel->groupContacts();
            publishContactsRP = publishChannel->groupLocalPendingContacts();
        }
    }

    if (!subscribeChannel && !publishChannel) {
        return;
    }

    QSet<QSharedPointer<Contact> > contacts = allKnownContacts();
    foreach (QSharedPointer<Contact> contact, contacts) {
        if (subscribeChannel) {
            // not in "subscribe" -> No, in "subscribe" lp -> Ask, in "subscribe" current -> Yes
            if (subscribeContacts.contains(contact)) {
                contact->setSubscriptionState(Contact::PresenceStateYes);
            } else if (subscribeContactsLP.contains(contact)) {
                contact->setSubscriptionState(Contact::PresenceStateAsk);
            } else {
                contact->setSubscriptionState(Contact::PresenceStateNo);
            }
        }

        if (publishChannel) {
            // not in "publish" -> No, in "subscribe" rp -> Ask, in "publish" current -> Yes
            if (publishContacts.contains(contact)) {
                contact->setPublishState(Contact::PresenceStateYes);
            } else if (publishContactsRP.contains(contact)) {
                contact->setPublishState(Contact::PresenceStateAsk);
            } else {
                contact->setPublishState(Contact::PresenceStateNo);
            }
        }
    }
}

}
}
