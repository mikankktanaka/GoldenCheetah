/*
 * Copyright (c) 2011 Mark Liversedge (liversedge@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _Gc_CalDAV_h
#define _Gc_CalDAV_h
#include "GoldenCheetah.h"

#include <QObject>

// send receive HTTPS requests
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

// set user and password
#include <QAuthenticator>
#include "Context.h"
#include "Athlete.h"

// work with calDAV protocol
// ...

// GC Settings and Ride Metrics
#include "Settings.h"
#include "MetricAggregator.h"

// update main ride calendar
#include "ICalendar.h"

// rideitem
#include "RideItem.h"
#include "RideFile.h"
#include "JsonRideFile.h"

// create a UUID
#include <QUuid>

class CalDAV : public QObject
{
    Q_OBJECT
    G_OBJECT

    enum action { Options, PropFind, Put, Get, Events, Report, None };
    typedef enum action ActionType;

public:
    CalDAV(Context *context);


public slots:

    // Query CalDAV server Options
    bool options();

    // Query CalDAV server Options
    bool propfind();

    // authentication (and refresh all events)
    bool download();

    // Query CalDAV server for events ...
    bool report();

    // Upload ride as a VEVENT
    bool upload(RideItem *rideItem);

    // Catch NAM signals ...
    void requestReply(QNetworkReply *reply);
    void userpass(QNetworkReply*r,QAuthenticator*a);
    void sslErrors(QNetworkReply*,QList<QSslError>&);

private:
    Context *context;
    QNetworkAccessManager *nam;
    ActionType mode;
};
#endif
