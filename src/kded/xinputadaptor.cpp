/*
 * This file is part of the KDE wacomtablet project. For copyright
 * information and license terms see the AUTHORS and COPYING files
 * in the top-level directory of this distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "debug.h"
#include "xinputadaptor.h"
#include "xinputproperty.h"
#include "x11input.h"
#include "x11inputdevice.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QtCore/QProcess>
#include <QtCore/QRegExp>

using namespace Wacom;

namespace Wacom {
class XinputAdaptorPrivate
{
    public:
        QString        deviceName;
        X11InputDevice device;
}; // CLASS
} // NAMESPACE


XinputAdaptor::XinputAdaptor(const QString& deviceName)
    : PropertyAdaptor(NULL), d_ptr(new XinputAdaptorPrivate)
{
    Q_D( XinputAdaptor );
    d->deviceName = deviceName;
    X11Input::findDevice(deviceName, d->device);
}


XinputAdaptor::~XinputAdaptor()
{
    delete this->d_ptr;
}


const QList< Property > XinputAdaptor::getProperties() const
{
    return XinputProperty::ids();
}


const QString XinputAdaptor::getProperty(const Property& property) const
{
    Q_D(const XinputAdaptor);

    const XinputProperty* xinputproperty = XinputProperty::map(property);

    if (xinputproperty == NULL) {
        kError() << QString::fromLatin1("Can not get unsupported property '%1' from device '%2' using xinput!").arg(property.key()).arg(d->deviceName);
        return QString();
    }

    if (!d->device.isOpen()) {
        kError() << QString::fromLatin1("Can not get property '%1' from device '%2' because the device is not available!").arg(property.key()).arg(d->deviceName);
        return QString();
    }

    return getProperty(*xinputproperty);
}


bool XinputAdaptor::setProperty(const Property& property, const QString& value) const
{
    Q_D(const XinputAdaptor);

    const XinputProperty* xinputproperty = XinputProperty::map(property);

    if (xinputproperty == NULL) {
        kError() << QString::fromLatin1("Can not set unsupported property '%1' to '%2' on device '%3' using xinput!").arg(property.key()).arg(value).arg(d->deviceName);
        return false;
    }

    if (!d->device.isOpen()) {
        kError() << QString::fromLatin1("Can not set property '%1' to '%2' on device '%3' because the device is not available!").arg(property.key()).arg(value).arg(d->deviceName);
        return false;
    }

    return setProperty(*xinputproperty, value);
}


bool XinputAdaptor::supportsProperty(const Property& property) const
{
    return (XinputProperty::map(property) != NULL);
}



const QString XinputAdaptor::getProperty(const XinputProperty& property) const
{
    if (property == XinputProperty::CursorAccelProfile) {
        return getLongProperty (property);

    } else if (property == XinputProperty::CursorAccelAdaptiveDeceleration) {
        return getFloatProperty (property);

    } else if (property == XinputProperty::CursorAccelConstantDeceleration) {
        return getFloatProperty (property);

    } else if (property == XinputProperty::CursorAccelVelocityScaling) {
        return getFloatProperty (property);

    } else {
        kError() << QString::fromLatin1("Getting Xinput property '%1' is not yet implemented!").arg(property.key());
    }

    return QString();
}



const QString XinputAdaptor::getFloatProperty(const XinputProperty& property, long nelements) const
{
    Q_D( const XinputAdaptor );

    QList<float> values;

    if (!d->device.getFloatProperty(property.key(), values, nelements)) {
        kError() << QString::fromLatin1("Failed to get Xinput property '%1' from device '%2'!").arg(property.key()).arg(d->deviceName);
        return QString();
    }

    return numbersToString<float>(values);
}



const QString XinputAdaptor::getLongProperty(const XinputProperty& property, long nelements) const
{
    Q_D( const XinputAdaptor );

    QList<long> values;

    if (!d->device.getLongProperty(property.key(), values, nelements)) {
        kError() << QString::fromLatin1("Failed to get Xinput property '%1' from device '%2'!").arg(property.key()).arg(d->deviceName);
        return QString();
    }

    return numbersToString<long>(values);
}



bool XinputAdaptor::mapTabletToScreen(const QString& screenArea) const
{
    Q_D( const XinputAdaptor );

    // what we need is the Coordinate Transformation Matrix
    // in the normal case where the whole screen is used we end up with a 3x3 identity matrix
    //in our case we want to change that
    // | w  0  offsetX |
    // | 0  h  offsetY |
    // | 0  0     1    |

    QStringList screenList = screenArea.split( QLatin1String( " " ) );

    if( screenList.isEmpty() || screenList.size() != 4 ) {
        kError() << "mapTabletToScreen :: can't parse ScreenSpace entry '" << screenArea << "' => device:" << d->deviceName;
        return false;
    }

    // read in what the user wants to use
    int screenX = screenList.at( 0 ).toInt();
    int screenY = screenList.at( 1 ).toInt();
    int screenW = screenList.at( 2 ).toInt();
    int screenH = screenList.at( 3 ).toInt();

    //use qt to create the real screen space available (the space that corresponse to the identity matrix

    QRectF virtualScreen = QRect( 0, 0, 0, 0 );

    int num = QApplication::desktop()->numScreens();

    for( int i = 0; i < num; i++ ) {
        QRect screen = QApplication::desktop()->screenGeometry( i );

        virtualScreen = virtualScreen.united( screen );
    }
    kDebug() << "virtual screen" << virtualScreen;

    // and now the values of the new matrix
    qreal w = ( qreal )screenW / ( qreal )virtualScreen.width();
    qreal h = ( qreal )screenH / ( qreal )virtualScreen.height();

    qreal offsetX = ( qreal )screenX / ( qreal )virtualScreen.width();
    qreal offsetY = ( qreal )screenY / ( qreal )virtualScreen.height();

    kDebug() << "Apply Coordinate Transformation Matrix";
    kDebug() << w << "0" << offsetX;
    kDebug() << "0" << h << offsetY;
    kDebug() << "0" << "0" << "1";

    return X11Input::setCoordinateTransformationMatrix(d->deviceName, offsetX, offsetY, w, h);
}



template<typename T>
const QString XinputAdaptor::numbersToString(const QList<T>& values) const
{
    QString result;

    for (int i = 0 ; i < values.size() ; ++i) {
        if (i > 0) {
            result += QLatin1String(" ");
        }

        result += QString::number(values.at(i));
    }

    return result;
}



bool XinputAdaptor::setProperty (const XinputProperty& property, const QString& value) const
{
    Q_D( const XinputAdaptor );

    if (property == XinputProperty::CursorAccelProfile) {
        return d->device.setLongProperty (property.key(), value);

    } else if (property == XinputProperty::CursorAccelAdaptiveDeceleration) {
        return d->device.setFloatProperty (property.key(), value);

    } else if (property == XinputProperty::CursorAccelConstantDeceleration) {
        return d->device.setFloatProperty (property.key(), value);

    } else if (property == XinputProperty::CursorAccelVelocityScaling) {
        return d->device.setFloatProperty (property.key(), value);

    } else if (property == XinputProperty::ScreenSpace) {
        return mapTabletToScreen (value);

    } else {
        kError() << QString::fromLatin1("Setting Xinput property '%1' is not yet implemented!").arg(property.key());
    }

    return false;
}

