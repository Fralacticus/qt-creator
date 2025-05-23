// Copyright (C) 2016 Jochen Becher
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "defaultstyleengine.h"

#include "defaultstyle.h"
#include "objectvisuals.h"
#include "relationvisuals.h"
#include "styledobject.h"
#include "styledrelation.h"

#include "qmt/diagram/dclass.h"
#include "qmt/diagram/dpackage.h"
#include "qmt/diagram/dcomponent.h"
#include "qmt/diagram/ditem.h"
#include "qmt/diagram/dannotation.h"
#include "qmt/infrastructure/qmtassert.h"
#include "qmt/stereotype/customrelation.h"

#include <utils/algorithm.h>

#include <QSet>

namespace {

class DepthProperties
{
public:
    DepthProperties() = default;
    DepthProperties(qmt::DefaultStyleEngine::ElementType elementType,
                    qmt::DObject::VisualPrimaryRole visualPrimaryRole,
                    qmt::DObject::VisualSecondaryRole visualSecondaryRole)
        : m_elementType(elementType),
          m_visualPrimaryRole(visualPrimaryRole),
          m_visualSecondaryRole(visualSecondaryRole)
    {
    }

    qmt::DefaultStyleEngine::ElementType m_elementType = qmt::DefaultStyleEngine::TypeOther;
    qmt::DObject::VisualPrimaryRole m_visualPrimaryRole = qmt::DObject::PrimaryRoleNormal;
    qmt::DObject::VisualSecondaryRole m_visualSecondaryRole = qmt::DObject::SecondaryRoleNone;
};

} // namespace

namespace qmt {

// TODO use tuple instead of these 4 explicit key classes

class ObjectStyleKey
{
public:
    ObjectStyleKey() = default;

    ObjectStyleKey(StyleEngine::ElementType elementType, const ObjectVisuals &objectVisuals)
        : m_elementType(elementType),
          m_objectVisuals(objectVisuals)
    {
    }

    StyleEngine::ElementType m_elementType = StyleEngine::TypeOther;
    ObjectVisuals m_objectVisuals;
};

size_t qHash(const ObjectStyleKey &styleKey)
{
    return ::qHash(styleKey.m_elementType) ^ qHash(styleKey.m_objectVisuals);
}

bool operator==(const ObjectStyleKey &lhs, const ObjectStyleKey &rhs)
{
    return lhs.m_elementType == rhs.m_elementType && lhs.m_objectVisuals == rhs.m_objectVisuals;
}

class RelationStyleKey
{
public:
    RelationStyleKey(StyleEngine::ElementType elementType, const RelationVisuals &relationVisuals,
                     bool withObject = false)
        : m_elementType(elementType),
          m_relationVisuals(relationVisuals),
        m_withObject(withObject)
    {
    }

    StyleEngine::ElementType m_elementType = StyleEngine::TypeOther;
    RelationVisuals m_relationVisuals;
    bool m_withObject = false;
};

size_t qHash(const RelationStyleKey &styleKey)
{
    return ::qHash(styleKey.m_elementType) ^ qHash(styleKey.m_relationVisuals);
}

bool operator==(const RelationStyleKey &lhs, const RelationStyleKey &rhs)
{
    return lhs.m_elementType == rhs.m_elementType
           && lhs.m_relationVisuals == rhs.m_relationVisuals
           && lhs.m_withObject == rhs.m_withObject;
}

class AnnotationStyleKey
{
public:
    AnnotationStyleKey(DAnnotation::VisualRole visualRole = DAnnotation::RoleNormal)
        : m_visualRole(visualRole)
    {
    }

    DAnnotation::VisualRole m_visualRole = DAnnotation::RoleNormal;
};

size_t qHash(const AnnotationStyleKey &styleKey)
{
    return ::qHash(styleKey.m_visualRole);
}

bool operator==(const AnnotationStyleKey &lhs, const AnnotationStyleKey &rhs)
{
    return lhs.m_visualRole == rhs.m_visualRole;
}

// TODO remove class if no attributes needed even with future extensions
class BoundaryStyleKey
{
};

size_t qHash(const BoundaryStyleKey &styleKey)
{
    Q_UNUSED(styleKey)

    return ::qHash(1);
}

bool operator==(const BoundaryStyleKey &lhs, const BoundaryStyleKey &rhs)
{
    Q_UNUSED(lhs)
    Q_UNUSED(rhs)

    return true;
}

// TODO remove class if no attributes needed even with future extensions
class SwimlaneStyleKey
{
};

size_t qHash(const SwimlaneStyleKey &styleKey)
{
    Q_UNUSED(styleKey)

    return ::qHash(1);
}

bool operator==(const SwimlaneStyleKey &lhs, const SwimlaneStyleKey &rhs)
{
    Q_UNUSED(lhs)
    Q_UNUSED(rhs)

    return true;
}

DefaultStyleEngine::DefaultStyleEngine()
{
}

DefaultStyleEngine::~DefaultStyleEngine()
{
    qDeleteAll(m_objectStyleMap);
    qDeleteAll(m_relationStyleMap);
    qDeleteAll(m_annotationStyleMap);
    qDeleteAll(m_boundaryStyleMap);
}

const Style *DefaultStyleEngine::applyStyle(const Style *baseStyle, StyleEngine::ElementType elementType,
                                            const StyleEngine::Parameters *parameters)
{
    switch (elementType) {
    case TypeAnnotation:
        return applyAnnotationStyle(baseStyle, DAnnotation::RoleNormal, parameters);
    case TypeBoundary:
        return applyBoundaryStyle(baseStyle, parameters);
    case TypeRelation:
        break;
    case TypeClass:
    case TypeComponent:
    case TypeItem:
    case TypePackage:
        return applyObjectStyle(
                    baseStyle, elementType,
                    ObjectVisuals(DObject::PrimaryRoleNormal, DObject::SecondaryRoleNone, false, QColor(), 0),
                    parameters);
    case TypeOther:
        break;
    case TypeSwimlane:
        return applySwimlaneStyle(baseStyle, parameters);
    }
    return baseStyle;
}

const Style *DefaultStyleEngine::applyObjectStyle(const Style *baseStyle, StyleEngine::ElementType elementType,
                                                  const ObjectVisuals &objectVisuals,
                                                  const StyleEngine::Parameters *parameters)
{
    ObjectStyleKey key(elementType, objectVisuals);
    const Style *derivedStyle = m_objectStyleMap.value(key);
    if (!derivedStyle) {
        int lineWidth = 1;

        QColor fillColor = DefaultStyleEngine::fillColor(elementType, objectVisuals);
        QColor lineColor = DefaultStyleEngine::lineColor(elementType, objectVisuals);
        QColor textColor = DefaultStyleEngine::textColor(elementType, objectVisuals);

        QFont normalFont = baseStyle->normalFont();
        QFont headerFont = baseStyle->normalFont();
        if (objectVisuals.isEmphasized()) {
            lineWidth = 2;
            headerFont.setBold(true);
        }

        auto style = new Style(baseStyle->type());
        QPen linePen = baseStyle->linePen();
        linePen.setColor(lineColor);
        linePen.setWidth(lineWidth);
        style->setLinePen(linePen);
        style->setInnerLinePen(linePen);
        style->setOuterLinePen(linePen);
        style->setExtraLinePen(linePen);
        style->setTextBrush(QBrush(textColor));
        if (objectVisuals.visualSecondaryRole() == DObject::SecondaryRoleOutline) {
            style->setFillBrush(QBrush(Qt::white));
        } else if (objectVisuals.visualSecondaryRole() == DObject::SecondaryRoleFlat) {
            style->setFillBrush(QBrush(fillColor));
        } else {
            if (!parameters->suppressGradients()) {
                QLinearGradient fillGradient(0.0, 0.0, 0.0, 1.0);
                fillGradient.setCoordinateMode(QGradient::ObjectBoundingMode);
                fillGradient.setColorAt(0.0, fillColor.lighter(110));
                fillGradient.setColorAt(1.0, fillColor.darker(110));
                style->setFillBrush(QBrush(fillGradient));
            } else {
                style->setFillBrush(QBrush(fillColor));
            }
        }
        if (objectVisuals.visualSecondaryRole() == DObject::SecondaryRoleOutline)
            style->setExtraFillBrush(QBrush(Qt::white));
        else if (objectVisuals.visualSecondaryRole() == DObject::SecondaryRoleFlat)
            style->setExtraFillBrush(QBrush(fillColor));
        else
            style->setExtraFillBrush(QBrush(fillColor.darker(120)));
        style->setNormalFont(normalFont);
        style->setSmallFont(baseStyle->smallFont());
        style->setHeaderFont(headerFont);
        m_objectStyleMap.insert(key, style);
        derivedStyle = style;
    }

    return derivedStyle;
}

const Style *DefaultStyleEngine::applyObjectStyle(const Style *baseStyle, const StyledObject &styledObject,
                                                  const Parameters *parameters)
{
    ElementType elementType = objectType(styledObject.object());

    // find colliding elements which best match visual appearance of styled object
    DObject::VisualPrimaryRole styledVisualPrimaryRole = styledObject.objectVisuals().visualPrimaryRole();
    DObject::VisualSecondaryRole styledVisualSecondaryRole = styledObject.objectVisuals().visualSecondaryRole();
    QHash<int, DepthProperties> depths;
    const QList<const DObject *> collidingObjectList = styledObject.collidingObjects();
    for (const DObject *collidingObject : collidingObjectList) {
        int collidingDepth = collidingObject->depth();
        if (collidingDepth < styledObject.object()->depth()) {
            ElementType collidingElementType = objectType(collidingObject);
            DObject::VisualPrimaryRole collidingVisualPrimaryRole = collidingObject->visualPrimaryRole();
            DObject::VisualSecondaryRole collidingVisualSecondaryRole = collidingObject->visualSecondaryRole();
            if (!depths.contains(collidingDepth)) {
                depths.insert(collidingDepth, DepthProperties(collidingElementType, collidingVisualPrimaryRole,
                                                              collidingVisualSecondaryRole));
            } else {
                bool updateProperties = false;
                DepthProperties properties = depths.value(collidingDepth);
                if (properties.m_elementType != elementType && collidingElementType == elementType) {
                    properties.m_elementType = collidingElementType;
                    properties.m_visualPrimaryRole = collidingVisualPrimaryRole;
                    properties.m_visualSecondaryRole = collidingVisualSecondaryRole;
                    updateProperties = true;
                } else if (properties.m_elementType == elementType && collidingElementType == elementType) {
                    if ((properties.m_visualPrimaryRole != styledVisualPrimaryRole
                         || properties.m_visualSecondaryRole != styledVisualSecondaryRole)
                            && collidingVisualPrimaryRole == styledVisualPrimaryRole
                            && collidingVisualSecondaryRole == styledVisualSecondaryRole) {
                        properties.m_visualPrimaryRole = collidingVisualPrimaryRole;
                        properties.m_visualSecondaryRole = collidingVisualSecondaryRole;
                        updateProperties = true;
                    }
                }
                if (updateProperties)
                    depths.insert(collidingDepth, properties);
            }
        }
    }
    int depth = 0;
    if (!depths.isEmpty()) {
        const QList<int> keys = Utils::sorted(depths.keys());
        for (int d : keys) {
            DepthProperties properties = depths.value(d);
            if (properties.m_elementType == elementType
                    && areStackingRoles(properties.m_visualPrimaryRole, properties.m_visualSecondaryRole,
                                        styledVisualPrimaryRole, styledVisualSecondaryRole)) {
                ++depth;
            } else {
                depth = 0;
            }
        }
    }

    return applyObjectStyle(baseStyle, elementType,
                            ObjectVisuals(styledVisualPrimaryRole,
                                          styledVisualSecondaryRole,
                                          styledObject.objectVisuals().isEmphasized(),
                                          styledObject.objectVisuals().baseColor(),
                                          depth),
                            parameters);
}

const Style *DefaultStyleEngine::applyRelationStyle(const Style *baseStyle, ElementType elementType, const RelationVisuals &relationVisuals, const Parameters *parameters)
{
    Q_UNUSED(parameters)

    RelationStyleKey key(elementType, relationVisuals);
    const Style *derivedStyle = m_relationStyleMap.value(key);
    if (!derivedStyle) {
        auto style = new Style(baseStyle->type());
        static QColor customColors[] = {
            QColor(0xEE, 0x8E, 0x99).darker(110),  // ROLE_CUSTOM1,
            QColor(0x80, 0xAF, 0x47).lighter(130), // ROLE_CUSTOM2,
            QColor(0xFF, 0xA1, 0x5B).lighter(100), // ROLE_CUSTOM3,
            QColor(0x55, 0xC4, 0xCF).lighter(120), // ROLE_CUSTOM4,
            QColor(0xFF, 0xE1, 0x4B)               // ROLE_CUSTOM5,
        };

        int index = static_cast<int>(relationVisuals.visualPrimaryRole()) - static_cast<int>(DRelation::PrimaryRoleCustom1);
        QColor lineColor = index >= 0 && index <= 4 ? customColors[index] : Qt::black;
        switch (relationVisuals.visualSecondaryRole()) {
        case DRelation::SecondaryRoleNone:
            break;
        case DRelation::SecondaryRoleWarning:
            lineColor = Qt::yellow;
            break;
        case DRelation::SecondaryRoleError:
            lineColor = Qt::red;
            break;
        case DRelation::SecondaryRoleSoften:
            lineColor = Qt::gray;
            break;
        }

        QColor fillColor = lineColor == Qt::black ? Qt::darkGray : lineColor.lighter(150);
        QPen linePen = baseStyle->linePen();
        linePen.setWidth(1);
        linePen.setColor(lineColor);
        style->setLinePen(linePen);
        QBrush textBrush = baseStyle->textBrush();
        textBrush.setColor(Qt::black);
        style->setTextBrush(textBrush);
        QBrush brush = baseStyle->fillBrush();
        brush.setColor(fillColor);
        brush.setStyle(Qt::SolidPattern);
        style->setFillBrush(brush);
        style->setNormalFont(baseStyle->normalFont());
        style->setSmallFont(baseStyle->smallFont());
        style->setHeaderFont(baseStyle->headerFont());
        m_relationStyleMap.insert(key, style);
        derivedStyle = style;
    }
    return derivedStyle;
}

const Style *DefaultStyleEngine::applyRelationStyle(const Style *baseStyle, const StyledRelation &styledRelation,
                                                    const Parameters *parameters)
{
    Q_UNUSED(parameters)

    ElementType elementType = objectType(styledRelation.endA());
    RelationVisuals relationVisuals;
    if (styledRelation.customRelation()) {
        switch (styledRelation.customRelation()->colorType()) {
        case CustomRelation::ColorType::EndA:
            // TODO implement
            break;
        case CustomRelation::ColorType::EndB:
            // TODO implement
            break;
        case CustomRelation::ColorType::Custom:
            // TODO implement
            break;
        case CustomRelation::ColorType::Warning:
            relationVisuals.setVisualSecondaryRole(DRelation::VisualSecondaryRole::SecondaryRoleWarning);
            break;
        case CustomRelation::ColorType::Error:
            relationVisuals.setVisualSecondaryRole(DRelation::VisualSecondaryRole::SecondaryRoleError);
            break;
        case CustomRelation::ColorType::Soften:
            relationVisuals.setVisualSecondaryRole(DRelation::VisualSecondaryRole::SecondaryRoleSoften);
            break;
        }
        relationVisuals.setEmphasized(styledRelation.customRelation()->emphasized());
    }
    if (styledRelation.endA())
        relationVisuals.setVisualObjectPrimaryRole(styledRelation.endA()->visualPrimaryRole());
    if (styledRelation.relation()) {
        if (styledRelation.relation()->visualPrimaryRole() != DRelation::VisualPrimaryRole::PrimaryRoleNormal)
            relationVisuals.setVisualPrimaryRole(styledRelation.relation()->visualPrimaryRole());
        if (styledRelation.relation()->visualSecondaryRole() != DRelation::VisualSecondaryRole::SecondaryRoleNone)
            relationVisuals.setVisualSecondaryRole(styledRelation.relation()->visualSecondaryRole());
        if (styledRelation.relation()->isVisualEmphasized())
            relationVisuals.setEmphasized(styledRelation.relation()->isVisualEmphasized());
    }
    RelationStyleKey key(elementType, relationVisuals, true);
    const Style *derivedStyle = m_relationStyleMap.value(key);
    if (!derivedStyle) {
        auto style = new Style(baseStyle->type());

        const DObject *object = styledRelation.endA();
        ObjectVisuals objectVisuals(object ? object->visualPrimaryRole() : DObject::PrimaryRoleNormal,
                                     object ? object->visualSecondaryRole() : DObject::SecondaryRoleNone,
                                     object ? object->isVisualEmphasized() : false,
                                     Qt::black, // TODO STyledRelation should get an EndAObjectVisuals
                                     object ? object->depth() : 0);
        QColor lineColor = DefaultStyleEngine::lineColor(objectType(object), objectVisuals);
        QColor fillColor = lineColor;

        static QColor customColors[] = {
            QColor(0xEE, 0x8E, 0x99).darker(110),  // ROLE_CUSTOM1,
            QColor(0x80, 0xAF, 0x47).lighter(130), // ROLE_CUSTOM2,
            QColor(0xFF, 0xA1, 0x5B).lighter(100), // ROLE_CUSTOM3,
            QColor(0x55, 0xC4, 0xCF).lighter(120), // ROLE_CUSTOM4,
            QColor(0xFF, 0xE1, 0x4B)               // ROLE_CUSTOM5,
        };

        int index = static_cast<int>(relationVisuals.visualPrimaryRole()) - static_cast<int>(DRelation::PrimaryRoleCustom1);
        lineColor = (index >= 0 && index <= 4) ? customColors[index] : lineColor;
        switch (relationVisuals.visualSecondaryRole()) {
        case DRelation::SecondaryRoleNone:
            break;
        case DRelation::SecondaryRoleWarning:
            lineColor = QColor(0xffc800);
            break;
        case DRelation::SecondaryRoleError:
            lineColor = Qt::red;
            break;
        case DRelation::SecondaryRoleSoften:
            lineColor = Qt::gray;
            break;
        }

        QPen linePen = baseStyle->linePen();
        linePen.setWidth(relationVisuals.isEmphasized() ? 3 : 1);
        linePen.setColor(lineColor);
        style->setLinePen(linePen);
        QBrush textBrush = baseStyle->textBrush();
        textBrush.setColor(Qt::black);
        style->setTextBrush(textBrush);
        QBrush brush = baseStyle->fillBrush();
        brush.setColor(fillColor);
        brush.setStyle(Qt::SolidPattern);
        style->setFillBrush(brush);
        style->setNormalFont(baseStyle->normalFont());
        style->setSmallFont(baseStyle->smallFont());
        style->setHeaderFont(baseStyle->headerFont());
        m_relationStyleMap.insert(key, style);
        derivedStyle = style;
    }
    return derivedStyle;
}

const Style *DefaultStyleEngine::applyAnnotationStyle(const Style *baseStyle, const DAnnotation *annotation,
                                                      const Parameters *parameters)
{
    DAnnotation::VisualRole visualRole = annotation ? annotation->visualRole() : DAnnotation::RoleNormal;
    return applyAnnotationStyle(baseStyle, visualRole, parameters);
}

const Style *DefaultStyleEngine::applyBoundaryStyle(const Style *baseStyle, const DBoundary *boundary,
                                                    const Parameters *parameters)
{
    Q_UNUSED(boundary)

    return applyBoundaryStyle(baseStyle, parameters);
}

const Style *DefaultStyleEngine::applySwimlaneStyle(const Style *baseStyle, const DSwimlane *swimlane, const StyleEngine::Parameters *parameters)
{
    Q_UNUSED(swimlane)

    return applySwimlaneStyle(baseStyle, parameters);
}

const Style *DefaultStyleEngine::applyAnnotationStyle(const Style *baseStyle, DAnnotation::VisualRole visualRole,
                                                      const StyleEngine::Parameters *parameters)
{
    Q_UNUSED(parameters)

    AnnotationStyleKey key(visualRole);
    const Style *derivedStyle = m_annotationStyleMap.value(key);
    if (!derivedStyle) {
        auto style = new Style(baseStyle->type());
        QFont normalFont;
        QBrush textBrush = baseStyle->textBrush();
        switch (visualRole) {
        case DAnnotation::RoleNormal:
            normalFont = baseStyle->normalFont();
            break;
        case DAnnotation::RoleTitle:
            normalFont = baseStyle->headerFont();
            break;
        case DAnnotation::RoleSubtitle:
            normalFont = baseStyle->normalFont();
            normalFont.setItalic(true);
            break;
        case DAnnotation::RoleEmphasized:
            normalFont = baseStyle->normalFont();
            normalFont.setBold(true);
            break;
        case DAnnotation::RoleSoften:
            normalFont = baseStyle->normalFont();
            textBrush.setColor(Qt::gray);
            break;
        case DAnnotation::RoleFootnote:
            normalFont = baseStyle->smallFont();
            break;
        }
        style->setNormalFont(normalFont);
        style->setTextBrush(textBrush);
        m_annotationStyleMap.insert(key, style);
        derivedStyle = style;
    }
    return derivedStyle;
}

const Style *DefaultStyleEngine::applyBoundaryStyle(const Style *baseStyle, const StyleEngine::Parameters *parameters)
{
    Q_UNUSED(parameters)

    BoundaryStyleKey key;
    const Style *derivedStyle = m_boundaryStyleMap.value(key);
    if (!derivedStyle) {
        auto style = new Style(baseStyle->type());
        style->setNormalFont(baseStyle->normalFont());
        style->setTextBrush(baseStyle->textBrush());
        m_boundaryStyleMap.insert(key, style);
        derivedStyle = style;
    }
    return derivedStyle;
}

const Style *DefaultStyleEngine::applySwimlaneStyle(const Style *baseStyle, const StyleEngine::Parameters *parameters)
{
    Q_UNUSED(parameters)

    SwimlaneStyleKey key;
    const Style *derivedStyle = m_swimlaneStyleMap.value(key);
    if (!derivedStyle) {
        auto style = new Style(baseStyle->type());
        style->setNormalFont(baseStyle->normalFont());
        style->setTextBrush(baseStyle->textBrush());
        m_swimlaneStyleMap.insert(key, style);
        derivedStyle = style;
    }
    return derivedStyle;
}

DefaultStyleEngine::ElementType DefaultStyleEngine::objectType(const DObject *object)
{
    ElementType elementType;
    if (dynamic_cast<const DPackage *>(object))
        elementType = TypePackage;
    else if (dynamic_cast<const DComponent *>(object))
        elementType = TypeComponent;
    else if (dynamic_cast<const DClass *>(object))
        elementType = TypeClass;
    else if (dynamic_cast<const DItem *>(object))
        elementType = TypeItem;
    else
        elementType = TypeOther;
    return elementType;
}

bool DefaultStyleEngine::areStackingRoles(DObject::VisualPrimaryRole rhsPrimaryRole,
                                          DObject::VisualSecondaryRole rhsSecondaryRole,
                                          DObject::VisualPrimaryRole lhsPrimaryRole,
                                          DObject::VisualSecondaryRole lhsSecondaryRols)
{
    switch (rhsSecondaryRole) {
    case DObject::SecondaryRoleNone:
    case DObject::SecondaryRoleLighter:
    case DObject::SecondaryRoleDarker:
    case DObject::SecondaryRoleFlat:
        switch (lhsSecondaryRols) {
        case DObject::SecondaryRoleNone:
        case DObject::SecondaryRoleLighter:
        case DObject::SecondaryRoleDarker:
        case DObject::SecondaryRoleFlat:
            return lhsPrimaryRole == rhsPrimaryRole;
        case DObject::SecondaryRoleSoften:
        case DObject::SecondaryRoleOutline:
            return false;
        }
        break;
    case DObject::SecondaryRoleSoften:
    case DObject::SecondaryRoleOutline:
        return false;
    }
    return true;
}

QColor DefaultStyleEngine::baseColor(ElementType elementType, ObjectVisuals objectVisuals)
{
    if (objectVisuals.visualSecondaryRole() == DObject::SecondaryRoleOutline)
        return QColor(0xFF, 0xFF, 0xFF);

    QColor baseColor;

    if (objectVisuals.visualPrimaryRole() == DObject::PrimaryRoleNormal) {
        if (objectVisuals.baseColor().isValid()) {
            baseColor = objectVisuals.baseColor();
        } else {
            switch (elementType) {
            case TypePackage:
                baseColor = QColor(0x7C, 0x98, 0xAD);
                break;
            case TypeComponent:
                baseColor = QColor(0xA0, 0xA8, 0x91);
                break;
            case TypeClass:
                baseColor = QColor(0xE5, 0xA8, 0x58);
                break;
            case TypeItem:
                baseColor = QColor(0xB9, 0x95, 0xC6);
                break;
            case TypeRelation:
            case TypeAnnotation:
            case TypeBoundary:
            case TypeSwimlane:
            case TypeOther:
                baseColor = QColor(0xBF, 0x7D, 0x65);
                break;
            }
        }
    } else {
        static QColor customColors[] = {
            QColor(0xEE, 0x8E, 0x99).darker(110),  // ROLE_CUSTOM1,
            QColor(0x80, 0xAF, 0x47).lighter(130), // ROLE_CUSTOM2,
            QColor(0xFF, 0xA1, 0x5B).lighter(100), // ROLE_CUSTOM3,
            QColor(0x55, 0xC4, 0xCF).lighter(120), // ROLE_CUSTOM4,
            QColor(0xFF, 0xE1, 0x4B)               // ROLE_CUSTOM5,
        };

        int index = static_cast<int>(objectVisuals.visualPrimaryRole()) - static_cast<int>(DObject::PrimaryRoleCustom1);
        QMT_ASSERT(index >= 0 && index <= 4, return baseColor);
        baseColor = customColors[index];
    }

    switch (objectVisuals.visualSecondaryRole()) {
    case DObject::SecondaryRoleNone:
        break;
    case DObject::SecondaryRoleLighter:
        baseColor = baseColor.lighter(110);
        break;
    case DObject::SecondaryRoleDarker:
        baseColor = baseColor.darker(120);
        break;
    case DObject::SecondaryRoleSoften:
        baseColor = baseColor.lighter(300);
        break;
    case DObject::SecondaryRoleOutline:
        QMT_CHECK(false);
        break;
    case DObject::SecondaryRoleFlat:
        break;
    }

    return baseColor;
}

QColor DefaultStyleEngine::lineColor(ElementType elementType, const ObjectVisuals &objectVisuals)
{
    QColor lineColor;
    if (objectVisuals.visualSecondaryRole() == DObject::SecondaryRoleOutline)
        lineColor = Qt::black;
    else if (objectVisuals.visualSecondaryRole() == DObject::SecondaryRoleSoften)
        lineColor = Qt::gray;
    else
        lineColor = baseColor(elementType, objectVisuals).darker(200).lighter(150).darker(100 + objectVisuals.depth() * 10);
    return lineColor;
}

QColor DefaultStyleEngine::fillColor(ElementType elementType, const ObjectVisuals &objectVisuals)
{
    QColor fillColor;
    if (objectVisuals.visualSecondaryRole() == DObject::SecondaryRoleOutline)
        fillColor = Qt::white;
    else
        fillColor = baseColor(elementType, objectVisuals).lighter(150).darker(100 + objectVisuals.depth() * 10);
    return fillColor;
}

QColor DefaultStyleEngine::textColor(const DObject *object, int depth)
{
    Q_UNUSED(depth)

    QColor textColor;
    DObject::VisualPrimaryRole visualRole = object ? object->visualPrimaryRole() : DObject::PrimaryRoleNormal;
    if (visualRole == DObject::DeprecatedPrimaryRoleSoften)
        textColor = Qt::gray;
    else
        textColor = Qt::black;
    return textColor;
}

QColor DefaultStyleEngine::textColor(ElementType elementType, const ObjectVisuals &objectVisuals)
{
    Q_UNUSED(elementType)

    QColor textColor;
    if (objectVisuals.visualSecondaryRole() == DObject::SecondaryRoleSoften)
        textColor = Qt::gray;
    else
        textColor = Qt::black;
    return textColor;
}

} // namespace qmt
