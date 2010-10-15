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

#ifndef _TelepathyQt4_contact_h_HEADER_GUARD_
#define _TelepathyQt4_contact_h_HEADER_GUARD_

#ifndef IN_TELEPATHY_QT4_HEADER
#error IN_TELEPATHY_QT4_HEADER
#endif

#include <QObject>
#include <QSet>
#include <QSharedPointer>
#include <QVariantMap>

#include <TelepathyQt4/Types>

namespace Tp
{

class ContactCapabilities;
class ContactLocation;
class ContactManager;
class PendingContactInfo;
class PendingOperation;
class ReferencedHandles;

class TELEPATHY_QT4_EXPORT Contact : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Contact);

public:
    enum Feature {
        FeatureAlias,
        FeatureAvatarToken,
        FeatureSimplePresence,
        FeatureCapabilities,
        FeatureLocation,
        FeatureInfo,
        FeatureAvatarData,
        _Padding = 0xFFFFFFFF
    };

    enum PresenceState {
         PresenceStateNo,
         PresenceStateAsk,
         PresenceStateYes
    };

    class InfoFields
    {
    public:
        InfoFields();
        InfoFields(const ContactInfoFieldList &fields);
        InfoFields(const InfoFields &other);
        ~InfoFields();

        bool isValid() const { return mPriv.constData() != 0; }

        InfoFields &operator=(const InfoFields &other);

        ContactInfoFieldList fields(const QString &name) const;

        ContactInfoFieldList allFields() const;

    private:
        friend class Contact;
        friend class PendingContactInfo;

        void setAllFields(const ContactInfoFieldList &fields);

        struct Private;
        friend struct Private;
        QSharedDataPointer<Private> mPriv;
    };

    ContactManager *manager() const;

    ReferencedHandles handle() const;
    QString id() const; // TODO filter: exact, prefix, substring match

    QSet<Feature> requestedFeatures() const;
    QSet<Feature> actualFeatures() const;

    QString alias() const; // TODO filter: exact, prefix, substring match

    bool isAvatarTokenKnown() const;
    QString avatarToken() const;
    AvatarData avatarData() const;

    /*
     * TODO filter:
     *  - exact match of presenceType, presenceStatus
     *  - ANY 1 of a number of presenceTypes/Statuses
     *  - presenceType greater or less than a set value
     */
    QString presenceStatus() const;
    uint presenceType() const;

    // TODO filter: have/don't have message AND exact/prefix/substring
    QString presenceMessage() const; 

    // TODO filter: the same as Account filtering by caps
    ContactCapabilities *capabilities() const;

    // TODO filter: is it available, how accurate, are they near me
    ContactLocation *location() const;

    // TODO filter: having a specific field, having ANY field,
    // (field: exact, contents: exact/prefix/substring)
    TELEPATHY_QT4_DEPRECATED ContactInfoFieldList info() const;
    InfoFields infoFields() const;
    PendingOperation *refreshInfo();
    PendingContactInfo *requestInfo();

    /*
     * Filters on exact values of these, but also the "in your contact list at all or not" usecase
     */
    PresenceState subscriptionState() const;
    PresenceState publishState() const;

    PendingOperation *requestPresenceSubscription(const QString &message = QString());
    PendingOperation *removePresenceSubscription(const QString &message = QString());
    PendingOperation *authorizePresencePublication(const QString &message = QString());
    PendingOperation *removePresencePublication(const QString &message = QString());

    /*
     * Filter on being blocked or not
     */
    bool isBlocked() const;
    PendingOperation *block(bool value = true);

    /*
     * Filter on the groups they're in - to show a specific group only
     *
     * Also prefix/substring match on ANY of the groups of the contact
     */
    QStringList groups() const;
    PendingOperation *addToGroup(const QString &group);
    PendingOperation *removeFromGroup(const QString &group);

    ~Contact();

Q_SIGNALS:
    void aliasChanged(const QString &alias);
    void avatarTokenChanged(const QString &avatarToken);
    void avatarDataChanged(const Tp::AvatarData &);
    void simplePresenceChanged(const QString &status, uint type, const QString &presenceMessage);
    void capabilitiesChanged(Tp::ContactCapabilities *caps);
    void locationUpdated(Tp::ContactLocation *location);
    void infoChanged(const Tp::ContactInfoFieldList &info);
    void infoFieldsChanged(const Tp::Contact::InfoFields &infoFields);

    void subscriptionStateChanged(Tp::Contact::PresenceState state);
    void publishStateChanged(Tp::Contact::PresenceState state);
    void blockStatusChanged(bool blocked);

    void addedToGroup(const QString &group);
    void removedFromGroup(const QString &group);

    // TODO: consider how the Renaming interface should work and map to Contacts
    // I guess it would be something like:
    // void renamedTo(Tp::ContactPtr)
    // with that contact getting the same features requested as the current one. Or would we rather
    // want to signal that change right away with a handle?

private:
    Contact(ContactManager *manager, const ReferencedHandles &handle,
            const QSet<Feature> &requestedFeatures, const QVariantMap &attributes);

    void augment(const QSet<Feature> &requestedFeatures, const QVariantMap &attributes);

    void receiveAlias(const QString &alias);
    void receiveAvatarToken(const QString &avatarToken);
    void setAvatarToken(const QString &token);
    void receiveAvatarData(const AvatarData &);
    void receiveSimplePresence(const SimplePresence &presence);
    void receiveCapabilities(const RequestableChannelClassList &caps);
    void receiveLocation(const QVariantMap &location);
    void receiveInfo(const ContactInfoFieldList &info);

    void setSubscriptionState(PresenceState state);
    void setPublishState(PresenceState state);
    void setBlocked(bool value);

    void setAddedToGroup(const QString &group);
    void setRemovedFromGroup(const QString &group);

    struct Private;
    friend class ContactManager;
    friend struct Private;
    Private *mPriv;
};

typedef QSet<ContactPtr> Contacts;

inline uint qHash(const ContactPtr &contact)
{
    return qHash(contact.data());
}

} // Tp

#endif
