
#include <iterator>
#include <algorithm>

#include <QDateTime>
#include <QPointF>
#include <QColor>
#include <QString>

#include <QDebug>

#include "reactvaluecoercion.h"
#include "reactmoduleinterface.h"
#include "reactbridge.h"
#include "reactnetworking.h"


// XXX: should have some way for modules to add these
QMap<int, coerce_function> coerceFunctions
{
  {
    qMetaTypeId<QDateTime>(),
    [](const QVariant& value) {
      Q_ASSERT(value.canConvert<double>());
      return QDateTime::fromMSecsSinceEpoch(value.toLongLong());
    }
  },
  {
    qMetaTypeId<QList<int> >(),
    [](const QVariant& value) {
      Q_ASSERT(value.canConvert<QVariantList>());
      QVariantList s = value.toList();
      QList<int> r;
      std::transform(s.begin(), s.end(), std::back_inserter(r), [](const QVariant& v) { return v.toInt(); });
      return QVariant::fromValue(r);
    }
  },
  {
    qMetaTypeId<QList<QString>>(),
    [](const QVariant& value) {
      Q_ASSERT(value.canConvert<QVariantList>());
      QVariantList s = value.toList();
      QList<QString> r;
      std::transform(s.begin(), s.end(),
                     std::back_inserter(r),
                     [](const QVariant& v) {
                       return v.toString();
                     });
      return QVariant::fromValue(r);
    }
  },
  {
    qMetaTypeId<QList<QList<QString>>>(),
    [](const QVariant& value) {
      Q_ASSERT(value.canConvert<QVariantList>());
      QVariantList s = value.toList();
      QList<QList<QString>> r;
      std::transform(s.begin(), s.end(),
                     std::back_inserter(r),
                     [](const QVariant& v) {
                       return v.toStringList();
                     });
      return QVariant::fromValue(r);
    }
  },
  {
    qRegisterMetaType<ReactModuleInterface::ListArgumentBlock>(),
    [](const QVariant& value) {
      Q_ASSERT(value.canConvert<int>());
      int callbackId = value.toInt();
      ReactModuleInterface::ResponseBlock block =
        [callbackId](ReactBridge* bridge, const QVariantList& args) {
          bridge->invokeAndProcess("invokeCallbackAndReturnFlushedQueue",
                                   QVariantList{ callbackId, args });
        };
      return QVariant::fromValue(block);
    }
  },
  {
    qRegisterMetaType<ReactModuleInterface::MapArgumentBlock>(),
    [](const QVariant& value) {
      Q_ASSERT(value.canConvert<int>());
      int callbackId = value.toInt();
      ReactModuleInterface::ErrorBlock block =
        [callbackId](ReactBridge* bridge, const QVariantMap& error) {
          bridge->invokeAndProcess("invokeCallbackAndReturnFlushedQueue",
                                   QVariantList{ callbackId, error });
        };
      return QVariant::fromValue(block);
    }
  },
  {
    qMetaTypeId<QPointF>(),
    [](const QVariant& value) {
      Q_ASSERT(value.canConvert<QVariantList>());
      QVariantList s = value.toList();
      Q_ASSERT(s.size() == 2);
      return QVariant::fromValue(QPointF(s[0].toDouble(), s[1].toDouble()));
    }
  },
  {
    qMetaTypeId<QColor>(),
    [](const QVariant& value) {
      Q_ASSERT(value.canConvert<uint>());
      return QVariant::fromValue(QColor::fromRgba(value.toUInt()));
    }
  }
};


QVariant reactCoerceValue(const QVariant& data, int parameterType, const coerce_map* userCoercions)
{
  if (!data.isValid() || data.isNull()) {
    return QVariant(parameterType, QMetaType::create(parameterType));
  }

  if (data.type() == parameterType || parameterType == QMetaType::QVariant) {
    return data;
  }

  coerce_function coerceFunction;

  // User supplied coercions first
  if (userCoercions != nullptr)
    coerceFunction = userCoercions->value(parameterType);

  // RN coercion functions
  if (!coerceFunction)
    coerceFunction = coerceFunctions.value(parameterType);

  if (coerceFunction)
    return coerceFunction(data);

  // QVariant coercions
  if (data.canConvert(parameterType)) {
    QVariant converted = data;
    converted.convert(parameterType);
    return converted;
  }

  return QVariant(); // Failure case; an invalid QVariant
}