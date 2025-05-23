// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "settingspage.h"

#include "designersettings.h"
#include "qmldesignerexternaldependencies.h"
#include "qmldesignerplugin.h"
#include "qmldesignertr.h"

#include <coreplugin/icore.h>

#include <qmljseditor/qmljseditorconstants.h>
#include <qmljstools/qmljstoolsconstants.h>

#include <qmlprojectmanager/qmlproject.h>

#include <utils/layoutbuilder.h>
#include <utils/pathchooser.h>
#include <utils/qtcassert.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTextStream>
#include <QVBoxLayout>

namespace QmlDesigner {
namespace Internal {

static QStringList puppetModes()
{
    static QStringList puppetModeList{"", "all", "editormode", "rendermode", "previewmode",
                                      "bakelightsmode", "import3dmode"};
    return puppetModeList;
}

class SettingsPageWidget final : public Core::IOptionsPageWidget
{
    Q_DECLARE_TR_FUNCTIONS(QmlDesigner::Internal::SettingsPage)

public:
    explicit SettingsPageWidget(ExternalDependencies &externalDependencies);

    void apply() final;

    QHash<QByteArray, QVariant> newSettings() const;
    void setSettings(const DesignerSettings &settings);

private:
    QSpinBox *m_spinItemSpacing;
    QSpinBox *m_spinSnapMargin;
    QSpinBox *m_spinCanvasHeight;
    QSpinBox *m_spinCanvasWidth;
    QCheckBox *m_smoothRendering;
    QSpinBox *m_spinRootItemInitHeight;
    QSpinBox *m_spinRootItemInitWidth;
    QLineEdit *m_styleLineEdit;
    QComboBox *m_controls2StyleComboBox;
    QGroupBox *m_qmlPuppetGroupBox;
    QRadioButton *m_useDefaultPuppetRadioButton;
    Utils::PathChooser *m_fallbackPuppetPathLineEdit;
    QRadioButton *m_useQtRelatedPuppetRadioButton;
    Utils::PathChooser *m_puppetBuildPathLineEdit;
    QCheckBox *m_alwaysSaveSubcomponentsCheckBox;
    QCheckBox *m_designerWarningsInEditorCheckBox;
    QCheckBox *m_designerWarningsCheckBox;
    QCheckBox *m_designerWarningsUiQmlfiles;
    QRadioButton *m_useQsTrFunctionRadioButton;
    QRadioButton *m_useQsTrIdFunctionRadioButton;
    QRadioButton *m_useQsTranslateFunctionRadioButton;
    QCheckBox *m_designerAlwaysDesignModeCheckBox;
    QCheckBox *m_askBeforeDeletingAssetCheckBox;
    QCheckBox *m_alwaysAutoFormatUICheckBox;
    QCheckBox *m_featureTimelineEditorCheckBox;
    QCheckBox *m_featureDockWidgetContentMinSize;
    QGroupBox *m_debugGroupBox;
    QCheckBox *m_designerShowDebuggerCheckBox;
    QCheckBox *m_showPropertyEditorWarningsCheckBox;
    QComboBox *m_forwardPuppetOutputComboBox;
    QCheckBox *m_designerEnableDebuggerCheckBox;
    QCheckBox *m_showWarnExceptionsCheckBox;
    QComboBox *m_debugPuppetComboBox;

    ExternalDependencies &m_externalDependencies;
};

SettingsPageWidget::SettingsPageWidget(ExternalDependencies &externalDependencies)
    : m_externalDependencies(externalDependencies)
{
    m_spinItemSpacing = new QSpinBox;
    m_spinItemSpacing->setMaximum(50);

    m_spinSnapMargin = new QSpinBox;
    m_spinSnapMargin->setMaximum(10);

    m_spinCanvasHeight = new QSpinBox;
    m_spinCanvasHeight->setMaximum(100000);
    m_spinCanvasHeight->setSingleStep(100);
    m_spinCanvasHeight->setValue(10000);

    m_spinCanvasWidth = new QSpinBox;
    m_spinCanvasWidth->setMaximum(100000);
    m_spinCanvasWidth->setSingleStep(100);
    m_spinCanvasWidth->setValue(10000);

    m_smoothRendering = new QCheckBox;
    m_smoothRendering->setToolTip(Tr::tr("Enable smooth rendering in the 2D view."));

    m_spinRootItemInitHeight = new QSpinBox;
    m_spinRootItemInitHeight->setMaximum(100000);
    m_spinRootItemInitHeight->setValue(480);

    m_spinRootItemInitWidth = new QSpinBox;
    m_spinRootItemInitWidth->setMaximum(100000);
    m_spinRootItemInitWidth->setValue(640);

    m_styleLineEdit = new QLineEdit;
    m_styleLineEdit->setPlaceholderText(Tr::tr("Default style"));

    auto resetStyle = new QPushButton(Tr::tr("Reset Style"));

    m_controls2StyleComboBox = new QComboBox;
    m_controls2StyleComboBox->addItems({ "Default", "Material", "Universal" });

    m_qmlPuppetGroupBox = new QGroupBox(Tr::tr("QML Puppet"));

    m_useDefaultPuppetRadioButton = new QRadioButton(Tr::tr("Use fallback QML Puppet"));
    m_useDefaultPuppetRadioButton->setToolTip(
        Tr::tr("If you select this radio button, Qt Design Studio always uses the "
               "QML Puppet located at the following path."));
    m_useDefaultPuppetRadioButton->setChecked(true);

    m_fallbackPuppetPathLineEdit = new Utils::PathChooser;
    m_fallbackPuppetPathLineEdit->setToolTip(
        Tr::tr("Path to the QML Puppet executable."));

    auto resetFallbackPuppetPathButton = new QPushButton(tr("Reset Path"));
    resetFallbackPuppetPathButton->setToolTip(
        Tr::tr("Resets the path to the built-in QML Puppet."));

    m_useQtRelatedPuppetRadioButton = new QRadioButton(
        Tr::tr("Use QML Puppet that is built with the selected Qt"));

    m_puppetBuildPathLineEdit = new Utils::PathChooser;
    m_puppetBuildPathLineEdit->setEnabled(false);

    auto resetQmlPuppetBuildPathButton = new QPushButton(Tr::tr("Reset Path"));

    m_alwaysSaveSubcomponentsCheckBox = new QCheckBox(
        Tr::tr("Always save when leaving subcomponent in bread crumb"));

    m_designerWarningsInEditorCheckBox = new QCheckBox(
        Tr::tr("Warn about unsupported features of .ui.qml files in code editor"));
    m_designerWarningsInEditorCheckBox->setToolTip(
        Tr::tr("Also warns in the code editor about QML features that are not properly "
               "supported by the Qt Quick Designer."));

    m_designerWarningsCheckBox = new QCheckBox(
        Tr::tr("Warn about unsupported features in .ui.qml files"));
    m_designerWarningsCheckBox->setToolTip(Tr::tr(
        "Warns about QML features that are not properly supported by the Qt Design Studio."));

    m_designerWarningsUiQmlfiles = new QCheckBox(
        Tr::tr("Warn about using .qml files instead of .ui.qml files"));
    m_designerWarningsUiQmlfiles->setToolTip(Tr::tr(
        "Qt Quick Designer will propose to open .ui.qml files instead of opening a .qml file."));

    m_useQsTrFunctionRadioButton = new QRadioButton(Tr::tr("qsTr()"));
    m_useQsTrFunctionRadioButton->setChecked(true);
    m_useQsTrIdFunctionRadioButton = new QRadioButton(Tr::tr("qsTrId()"));
    m_useQsTranslateFunctionRadioButton = new QRadioButton(Tr::tr("qsTranslate()"));

    m_designerAlwaysDesignModeCheckBox = new QCheckBox(
        Tr::tr("Always open ui.qml files in Design mode"));
    m_askBeforeDeletingAssetCheckBox = new QCheckBox(
        Tr::tr("Ask for confirmation before deleting asset"));
    m_alwaysAutoFormatUICheckBox = new QCheckBox(
        Tr::tr("Always auto-format ui.qml files in Design mode"));
    m_featureTimelineEditorCheckBox = new QCheckBox(Tr::tr("Enable Timeline editor"));
    m_featureDockWidgetContentMinSize = new QCheckBox(
        Tr::tr("Enable DockWidget content minimum size"));

    m_debugGroupBox = new QGroupBox(Tr::tr("Debugging"));
    m_designerShowDebuggerCheckBox = new QCheckBox(Tr::tr("Show the debugging view"));
    m_showPropertyEditorWarningsCheckBox = new QCheckBox(Tr::tr("Show property editor warnings"));

    m_forwardPuppetOutputComboBox = new QComboBox;

    m_designerEnableDebuggerCheckBox = new QCheckBox(Tr::tr("Enable the debugging view"));
    m_showWarnExceptionsCheckBox = new QCheckBox(Tr::tr("Show warn exceptions"));

    m_debugPuppetComboBox = new QComboBox;

    using namespace Layouting;

    Column{m_useDefaultPuppetRadioButton,
           Row{Space(20),
               Form{Tr::tr("Path:"), m_fallbackPuppetPathLineEdit, resetFallbackPuppetPathButton}},
           m_useQtRelatedPuppetRadioButton,
           Row{Space(20),
               Form{Tr::tr("Top level build path:"),
                    m_puppetBuildPathLineEdit,
                    resetQmlPuppetBuildPathButton}}}
        .attachTo(m_qmlPuppetGroupBox);

    Grid{m_designerShowDebuggerCheckBox,
         m_showPropertyEditorWarningsCheckBox,
         Form{Tr::tr("Forward QML Puppet output:"), m_forwardPuppetOutputComboBox},
         br,
         m_designerEnableDebuggerCheckBox,
         m_showWarnExceptionsCheckBox,
         Form{Tr::tr("Debug QML Puppet:"), m_debugPuppetComboBox}}
        .attachTo(m_debugGroupBox);

    Column{Row{Group{title(Tr::tr("Snapping")),
                     Form{Tr::tr("Parent component padding:"),
                          m_spinSnapMargin,
                          br,
                          Tr::tr("Sibling component spacing:"),
                          m_spinItemSpacing}},
               Group{title(Tr::tr("Canvas")),
                     Form{Tr::tr("Width:"),
                          m_spinCanvasWidth,
                          br,
                          Tr::tr("Height:"),
                          m_spinCanvasHeight,
                          br,
                          Tr::tr("Smooth rendering:"),
                          m_smoothRendering}},
               Group{title(Tr::tr("Root Component Init Size")),
                     Form{Tr::tr("Width:"),
                          m_spinRootItemInitWidth,
                          br,
                          Tr::tr("Height:"),
                          m_spinRootItemInitHeight}},
               Group{title(Tr::tr("Styling")),
                     Form{Tr::tr("Controls style:"),
                          m_styleLineEdit,
                          resetStyle,
                          br,
                          Tr::tr("Controls 2 style:"),
                          m_controls2StyleComboBox}}},
           m_qmlPuppetGroupBox,
           Group{title(Tr::tr("Subcomponents")), Column{m_alwaysSaveSubcomponentsCheckBox}},
           Row{Group{title(Tr::tr("Warnings")),
                     Column{m_designerWarningsCheckBox,
                            m_designerWarningsInEditorCheckBox,
                            m_designerWarningsUiQmlfiles}},
               Group{title(Tr::tr("Internationalization")),
                     Column{m_useQsTrFunctionRadioButton,
                            m_useQsTrIdFunctionRadioButton,
                            m_useQsTranslateFunctionRadioButton}}},
           Group{title(Tr::tr("Features")),
                 Grid{m_designerAlwaysDesignModeCheckBox,
                      m_alwaysAutoFormatUICheckBox,
                      br,
                      m_askBeforeDeletingAssetCheckBox,
                      m_featureTimelineEditorCheckBox,
                      br,
                      m_featureDockWidgetContentMinSize}},
           m_debugGroupBox,
           st}
        .attachTo(this);

    connect(m_designerEnableDebuggerCheckBox, &QCheckBox::toggled, [this](bool checked) {
        if (checked && ! m_designerShowDebuggerCheckBox->isChecked())
            m_designerShowDebuggerCheckBox->setChecked(true);
        }
    );
    connect(resetFallbackPuppetPathButton, &QPushButton::clicked, [&]() {
        m_fallbackPuppetPathLineEdit->setPath(externalDependencies.defaultPuppetFallbackDirectory());
    });
    m_fallbackPuppetPathLineEdit->lineEdit()->setPlaceholderText(
        externalDependencies.defaultPuppetFallbackDirectory());

    connect(resetQmlPuppetBuildPathButton, &QPushButton::clicked, [&]() {
        m_puppetBuildPathLineEdit->setPath(
            externalDependencies.defaultPuppetToplevelBuildDirectory());
    });
    connect(m_useDefaultPuppetRadioButton, &QRadioButton::toggled,
        m_fallbackPuppetPathLineEdit, &QLineEdit::setEnabled);
    connect(m_useQtRelatedPuppetRadioButton, &QRadioButton::toggled,
        m_puppetBuildPathLineEdit, &QLineEdit::setEnabled);
    connect(resetStyle, &QPushButton::clicked,
        m_styleLineEdit, &QLineEdit::clear);
    connect(m_controls2StyleComboBox, &QComboBox::currentTextChanged, [this] {
        m_styleLineEdit->setText(m_controls2StyleComboBox->currentText());
    });

    m_forwardPuppetOutputComboBox->addItems(puppetModes());
    m_debugPuppetComboBox->addItems(puppetModes());

    setSettings(QmlDesignerPlugin::instance()->settings());
}

QHash<QByteArray, QVariant> SettingsPageWidget::newSettings() const
{
    QHash<QByteArray, QVariant> settings;
    settings.insert(DesignerSettingsKey::ITEMSPACING, m_spinItemSpacing->value());
    settings.insert(DesignerSettingsKey::CONTAINERPADDING, m_spinSnapMargin->value());
    settings.insert(DesignerSettingsKey::CANVASWIDTH, m_spinCanvasWidth->value());
    settings.insert(DesignerSettingsKey::CANVASHEIGHT, m_spinCanvasHeight->value());
    settings.insert(DesignerSettingsKey::ROOT_ELEMENT_INIT_WIDTH, m_spinRootItemInitWidth->value());
    settings.insert(DesignerSettingsKey::ROOT_ELEMENT_INIT_HEIGHT, m_spinRootItemInitHeight->value());
    settings.insert(DesignerSettingsKey::WARNING_FOR_FEATURES_IN_DESIGNER,
                    m_designerWarningsCheckBox->isChecked());
    settings.insert(DesignerSettingsKey::WARNING_FOR_QML_FILES_INSTEAD_OF_UIQML_FILES,
                    m_designerWarningsUiQmlfiles->isChecked());

    settings.insert(DesignerSettingsKey::WARNING_FOR_DESIGNER_FEATURES_IN_EDITOR,
        m_designerWarningsInEditorCheckBox->isChecked());
    settings.insert(DesignerSettingsKey::SHOW_DEBUGVIEW,
        m_designerShowDebuggerCheckBox->isChecked());
    settings.insert(DesignerSettingsKey::ENABLE_DEBUGVIEW,
        m_designerEnableDebuggerCheckBox->isChecked());
    settings.insert(DesignerSettingsKey::USE_DEFAULT_PUPPET,
        m_useDefaultPuppetRadioButton->isChecked());

    int typeOfQsTrFunction;

    if (m_useQsTrFunctionRadioButton->isChecked())
        typeOfQsTrFunction = 0;
    else if (m_useQsTrIdFunctionRadioButton->isChecked())
        typeOfQsTrFunction = 1;
    else if (m_useQsTranslateFunctionRadioButton->isChecked())
        typeOfQsTrFunction = 2;
    else
        typeOfQsTrFunction = 0;

    settings.insert(DesignerSettingsKey::TYPE_OF_QSTR_FUNCTION, typeOfQsTrFunction);
    settings.insert(DesignerSettingsKey::CONTROLS_STYLE, m_styleLineEdit->text());
    settings.insert(DesignerSettingsKey::FORWARD_PUPPET_OUTPUT,
        m_forwardPuppetOutputComboBox->currentText());
    settings.insert(DesignerSettingsKey::DEBUG_PUPPET,
        m_debugPuppetComboBox->currentText());

    QString newFallbackPuppetPath = m_fallbackPuppetPathLineEdit->filePath().toUrlishString();
    QTC_CHECK(m_externalDependencies.defaultPuppetFallbackDirectory()
              == m_fallbackPuppetPathLineEdit->lineEdit()->placeholderText());
    if (newFallbackPuppetPath.isEmpty())
        newFallbackPuppetPath = m_fallbackPuppetPathLineEdit->lineEdit()->placeholderText();

    QString oldFallbackPuppetPath = m_externalDependencies.qmlPuppetFallbackDirectory();

    if (oldFallbackPuppetPath != newFallbackPuppetPath && QFileInfo::exists(newFallbackPuppetPath)) {
        if (newFallbackPuppetPath == m_externalDependencies.defaultPuppetFallbackDirectory())
            settings.insert(DesignerSettingsKey::PUPPET_DEFAULT_DIRECTORY, QString());
        else
            settings.insert(DesignerSettingsKey::PUPPET_DEFAULT_DIRECTORY, newFallbackPuppetPath);
    } else if (!QFileInfo::exists(oldFallbackPuppetPath) || !QFileInfo::exists(newFallbackPuppetPath)){
        settings.insert(DesignerSettingsKey::PUPPET_DEFAULT_DIRECTORY, QString());
    }

    if (!m_puppetBuildPathLineEdit->filePath().isEmpty()
        && m_puppetBuildPathLineEdit->filePath().toUrlishString()
               != m_externalDependencies.defaultPuppetToplevelBuildDirectory()) {
        settings.insert(DesignerSettingsKey::PUPPET_TOPLEVEL_BUILD_DIRECTORY,
            m_puppetBuildPathLineEdit->filePath().toUrlishString());
    }
    settings.insert(DesignerSettingsKey::ALWAYS_SAVE_IN_CRUMBLEBAR,
        m_alwaysSaveSubcomponentsCheckBox->isChecked());
    settings.insert(DesignerSettingsKey::SHOW_PROPERTYEDITOR_WARNINGS,
        m_showPropertyEditorWarningsCheckBox->isChecked());
    settings.insert(DesignerSettingsKey::ENABLE_MODEL_EXCEPTION_OUTPUT,
        m_showWarnExceptionsCheckBox->isChecked());
    settings.insert(DesignerSettingsKey::ENABLE_TIMELINEVIEW,
                    m_featureTimelineEditorCheckBox->isChecked());
    settings.insert(DesignerSettingsKey::ENABLE_DOCKWIDGET_CONTENT_MIN_SIZE,
                    m_featureDockWidgetContentMinSize->isChecked());
    settings.insert(DesignerSettingsKey::ALWAYS_DESIGN_MODE,
                    m_designerAlwaysDesignModeCheckBox->isChecked());
    settings.insert(DesignerSettingsKey::ASK_BEFORE_DELETING_ASSET,
                    m_askBeforeDeletingAssetCheckBox->isChecked());
    settings.insert(DesignerSettingsKey::SMOOTH_RENDERING, m_smoothRendering->isChecked());

    settings.insert(DesignerSettingsKey::REFORMAT_UI_QML_FILES,
                    m_alwaysAutoFormatUICheckBox->isChecked());

    return settings;
}

void SettingsPageWidget::setSettings(const DesignerSettings &settings)
{
    m_spinItemSpacing->setValue(settings.value(DesignerSettingsKey::ITEMSPACING).toInt());
    m_spinSnapMargin->setValue(settings.value(
        DesignerSettingsKey::CONTAINERPADDING).toInt());
    m_spinCanvasWidth->setValue(settings.value(
        DesignerSettingsKey::CANVASWIDTH).toInt());
    m_spinCanvasHeight->setValue(settings.value(
        DesignerSettingsKey::CANVASHEIGHT).toInt());
    m_spinRootItemInitWidth->setValue(settings.value(
        DesignerSettingsKey::ROOT_ELEMENT_INIT_WIDTH).toInt());
    m_spinRootItemInitHeight->setValue(settings.value(
        DesignerSettingsKey::ROOT_ELEMENT_INIT_HEIGHT).toInt());
    m_designerWarningsCheckBox->setChecked(settings.value(
        DesignerSettingsKey::WARNING_FOR_FEATURES_IN_DESIGNER).toBool());
    m_designerWarningsUiQmlfiles->setChecked(settings.value(
        DesignerSettingsKey::WARNING_FOR_QML_FILES_INSTEAD_OF_UIQML_FILES).toBool());
    m_designerWarningsInEditorCheckBox->setChecked(settings.value(
        DesignerSettingsKey::WARNING_FOR_DESIGNER_FEATURES_IN_EDITOR).toBool());
    m_designerShowDebuggerCheckBox->setChecked(settings.value(
        DesignerSettingsKey::SHOW_DEBUGVIEW).toBool());
    m_designerEnableDebuggerCheckBox->setChecked(settings.value(
        DesignerSettingsKey::ENABLE_DEBUGVIEW).toBool());
    m_useDefaultPuppetRadioButton->setChecked(settings.value(
        DesignerSettingsKey::USE_DEFAULT_PUPPET).toBool());
    m_useQtRelatedPuppetRadioButton->setChecked(!settings.value(
        DesignerSettingsKey::USE_DEFAULT_PUPPET).toBool());
    m_useQsTrFunctionRadioButton->setChecked(settings.value(
        DesignerSettingsKey::TYPE_OF_QSTR_FUNCTION).toInt() == 0);
    m_useQsTrIdFunctionRadioButton->setChecked(settings.value(
        DesignerSettingsKey::TYPE_OF_QSTR_FUNCTION).toInt() == 1);
    m_useQsTranslateFunctionRadioButton->setChecked(settings.value(
        DesignerSettingsKey::TYPE_OF_QSTR_FUNCTION).toInt() == 2);
    m_styleLineEdit->setText(settings.value(
        DesignerSettingsKey::CONTROLS_STYLE).toString());

    QString puppetFallbackDirectory = settings
                                          .value(DesignerSettingsKey::PUPPET_DEFAULT_DIRECTORY,
                                                 m_externalDependencies.defaultPuppetFallbackDirectory())
                                          .toString();
    m_fallbackPuppetPathLineEdit->setPath(puppetFallbackDirectory);

    QString puppetToplevelBuildDirectory = settings
                                               .value(DesignerSettingsKey::PUPPET_TOPLEVEL_BUILD_DIRECTORY,
                                                      m_externalDependencies
                                                          .defaultPuppetToplevelBuildDirectory())
                                               .toString();
    m_puppetBuildPathLineEdit->setPath(puppetToplevelBuildDirectory);

    m_forwardPuppetOutputComboBox->setCurrentText(settings.value(
        DesignerSettingsKey::FORWARD_PUPPET_OUTPUT).toString());
    m_debugPuppetComboBox->setCurrentText(settings.value(
        DesignerSettingsKey::DEBUG_PUPPET).toString());

    m_alwaysSaveSubcomponentsCheckBox->setChecked(settings.value(
        DesignerSettingsKey::ALWAYS_SAVE_IN_CRUMBLEBAR).toBool());
    m_showPropertyEditorWarningsCheckBox->setChecked(settings.value(
        DesignerSettingsKey::SHOW_PROPERTYEDITOR_WARNINGS).toBool());
    m_showWarnExceptionsCheckBox->setChecked(settings.value(
        DesignerSettingsKey::ENABLE_MODEL_EXCEPTION_OUTPUT).toBool());

    m_controls2StyleComboBox->setCurrentText(m_styleLineEdit->text());

    m_designerAlwaysDesignModeCheckBox->setChecked(settings.value(
        DesignerSettingsKey::ALWAYS_DESIGN_MODE).toBool());
    m_featureTimelineEditorCheckBox->setChecked(settings.value(
        DesignerSettingsKey::ENABLE_TIMELINEVIEW).toBool());
    m_featureDockWidgetContentMinSize->setChecked(
        settings.value(DesignerSettingsKey::ENABLE_DOCKWIDGET_CONTENT_MIN_SIZE).toBool());

    m_askBeforeDeletingAssetCheckBox->setChecked(
        settings.value(DesignerSettingsKey::ASK_BEFORE_DELETING_ASSET).toBool());

#ifdef QT_DEBUG
    const auto showDebugSettings = true;
#else
    const auto showDebugSettings = settings.value(DesignerSettingsKey::SHOW_DEBUG_SETTINGS).toBool();
#endif
    const bool showAdvancedFeatures = !Core::ICore::isQtDesignStudio() || showDebugSettings;
    m_qmlPuppetGroupBox->setVisible(showAdvancedFeatures);
    m_debugGroupBox->setVisible(showAdvancedFeatures);
    m_featureTimelineEditorCheckBox->setVisible(Core::ICore::isQtDesignStudio());
    m_featureDockWidgetContentMinSize->setVisible(Core::ICore::isQtDesignStudio());
    m_smoothRendering->setChecked(settings.value(DesignerSettingsKey::SMOOTH_RENDERING).toBool());

    m_alwaysAutoFormatUICheckBox->setChecked(
        settings.value(DesignerSettingsKey::REFORMAT_UI_QML_FILES).toBool());
}

void SettingsPageWidget::apply()
{
    auto settings = newSettings();

    const auto restartNecessaryKeys = {DesignerSettingsKey::PUPPET_DEFAULT_DIRECTORY,
                                       DesignerSettingsKey::PUPPET_TOPLEVEL_BUILD_DIRECTORY,
                                       DesignerSettingsKey::ENABLE_MODEL_EXCEPTION_OUTPUT,
                                       DesignerSettingsKey::FORWARD_PUPPET_OUTPUT,
                                       DesignerSettingsKey::DEBUG_PUPPET,
                                       DesignerSettingsKey::ENABLE_MODEL_EXCEPTION_OUTPUT,
                                       DesignerSettingsKey::ENABLE_TIMELINEVIEW,
                                       DesignerSettingsKey::ENABLE_DOCKWIDGET_CONTENT_MIN_SIZE};

    for (const char * const key : restartNecessaryKeys) {
        if (QmlDesignerPlugin::settings().value(key) != settings.value(key)) {
            QMessageBox::information(Core::ICore::dialogParent(),
                                     Tr::tr("Restart Required"),
                                     Tr::tr("The made changes will take effect after a "
                                            "restart of the QML Puppet or %1.")
                                         .arg(QGuiApplication::applicationDisplayName()));
            break;
        }
    }

    QmlDesignerPlugin::settings().insert(settings);
}

SettingsPage::SettingsPage(ExternalDependencies &externalDependencies)
{
    setId("B.QmlDesigner");
    setDisplayName(Tr::tr("Qt Quick Designer"));
    setCategory(QmlJSEditor::Constants::SETTINGS_CATEGORY_QML);
    setWidgetCreator([&] { return new SettingsPageWidget(externalDependencies); });
}

} // Internal
} // QmlDesigner
