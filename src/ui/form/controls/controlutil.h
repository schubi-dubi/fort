#ifndef CONTROLUTIL_H
#define CONTROLUTIL_H

#include <QBoxLayout>
#include <QObject>
#include <QVector>

#include <functional>

QT_FORWARD_DECLARE_CLASS(QAction)
QT_FORWARD_DECLARE_CLASS(QCheckBox)
QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QFormLayout)
QT_FORWARD_DECLARE_CLASS(QFrame)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QLayout)
QT_FORWARD_DECLARE_CLASS(QLineEdit)
QT_FORWARD_DECLARE_CLASS(QMenu)
QT_FORWARD_DECLARE_CLASS(QPushButton)
QT_FORWARD_DECLARE_CLASS(QSpinBox)
QT_FORWARD_DECLARE_CLASS(QToolButton)

class LabelColor;
class LabelDoubleSpin;
class LabelSpin;
class LabelSpinCombo;

class ControlUtil
{
public:
    static QCheckBox *createCheckBox(const QString &iconPath = {});
    static QCheckBox *createCheckBox(
            bool checked, const std::function<void(bool checked)> &onToggled);
    static QCheckBox *createCheckStateBox(
            const QString &iconPath, Qt::CheckState state, const std::function<void()> &onClicked);

    static QSpinBox *createSpinBox();

    static QComboBox *createComboBox(const QStringList &texts = {});
    static QComboBox *createComboBox(
            const QStringList &texts, const std::function<void(int index)> &onActivated);

    static void setComboBoxTexts(
            QComboBox *c, const QStringList &texts = {}, int currentIndex = -2);
    static void setComboBoxIcons(QComboBox *c, const QStringList &iconPaths);

    static void setComboBoxItemsEnabled(
            QComboBox *c, bool enabled, int fromIndex, int toIndex = -1);

    static QPushButton *createButton(const QString &iconPath, const QString &text = QString());
    static QPushButton *createButton(
            const QString &iconPath, const std::function<void()> &onClicked);

    static QToolButton *createSideButton(
            const QString &iconPath, const std::function<void()> &onClicked);

    static QToolButton *createToolButton(const QString &iconPath);
    static QToolButton *createToolButton(
            const QString &iconPath, const std::function<void()> &onClicked);

    static QToolButton *createFlatToolButton(const QString &iconPath);
    static QToolButton *createFlatToolButton(
            const QString &iconPath, const std::function<void()> &onClicked);

    static QToolButton *createIconToolButton(const QString &iconPath);
    static QToolButton *createIconToolButton(
            const QString &iconPath, const std::function<void()> &onClicked);

    static QToolButton *createSplitterButton(
            const QString &iconPath, const std::function<void()> &onClicked);

    static QLabel *createLabel(const QString &text = {});
    static QLabel *createIconLabel(const QString &iconPath, const QSize &size);
    static QLineEdit *createLineLabel();
    static QLineEdit *createLineEdit(
            const QString &text, const std::function<void(const QString &text)> &onChanged);

    static QMenu *createMenu(QWidget *parent = nullptr);
    static QMenu *createMenuByLayout(QBoxLayout *layout, QWidget *parent);

    static QBoxLayout *createLayout(QBoxLayout::Direction direction, int margin = 0);
    inline static QBoxLayout *createHLayout(int margin = 0)
    {
        return createLayout(QBoxLayout::LeftToRight, margin);
    }
    inline static QBoxLayout *createVLayout(int margin = 0)
    {
        return createLayout(QBoxLayout::TopToBottom, margin);
    }

    static QLayout *createRowLayout(QWidget *w1, QWidget *w2, int stretch1 = 1);

    static void fillLayoutByWidgets(QBoxLayout *layout, const QList<QWidget *> &widgets);

    static QBoxLayout *createLayoutByWidgets(
            const QList<QWidget *> &widgets, QBoxLayout::Direction direction, int margin = -1);

    inline static QBoxLayout *createHLayoutByWidgets(
            const QList<QWidget *> &widgets, int margin = -1)
    {
        return createLayoutByWidgets(widgets, QBoxLayout::LeftToRight, margin);
    }
    inline static QBoxLayout *createVLayoutByWidgets(
            const QList<QWidget *> &widgets, int margin = -1)
    {
        return createLayoutByWidgets(widgets, QBoxLayout::TopToBottom, margin);
    }

    static QFrame *createSeparator(Qt::Orientation o = Qt::Horizontal);

    inline static QFrame *createHSeparator() { return createSeparator(Qt::Horizontal); }
    inline static QFrame *createVSeparator() { return createSeparator(Qt::Vertical); }

    static void clearLayout(QLayout *layout);

    static QLayout *createScrollLayout(QLayout *content, bool isBgTransparent = true);
    static QWidget *wrapToScrollArea(QWidget *content, bool isBgTransparent = true);

    static LabelSpinCombo *createSpinCombo(int v, int min, int max, const QVector<int> &values,
            const QString &suffix, const std::function<void(int value)> &onValueChanged);
    static LabelSpin *createSpin(int v, int min, int max, const QString &suffix,
            const std::function<void(int value)> &onValueChanged);
    static LabelDoubleSpin *createDoubleSpin(double v, double min, double max,
            const QString &suffix, const std::function<void(double value)> &onValueChanged);

    static LabelColor *createLabelColor(const QColor &color, const QColor &darkColor,
            const std::function<void(const QColor &color)> &onColorChanged,
            const std::function<void(const QColor &color)> &onDarkColorChanged);

    static QLabel *formRowLabel(QFormLayout *formLayout, QWidget *field);
    static QLabel *formRowLabel(QFormLayout *formLayout, QLayout *field);

    static QPushButton *createMenuButton();
    static QToolButton *createOptionsButton(int tabIndex = 0);

    static void deleteOnClose(QWidget *w);
};

#endif // CONTROLUTIL_H
