#ifndef PRINTDIALOG_H
#define PRINTDIALOG_H

#include <QDialog>
#include <QPrinter>
#include <QPrintPreviewWidget>
#include <vector>
#include <QImage>
#include <functional>

struct PrintLayoutConfig {
    int TL;
    int TS;
    int PS;
    int TX;
    int TY;
    int PX;
    int PY;
    int STX;
    int STY;
    int XO;
};

class QComboBox;
class QPushButton;
class QLabel;
class QSpinBox;
class QCheckBox;
class QSlider;

class PrintDialog : public QDialog {
    Q_OBJECT

public:
    explicit PrintDialog(
        const std::vector<QImage>& images,
        const QString &configSummary,
        const PrintLayoutConfig &initialLayout,
        std::function<std::vector<QImage>(const PrintLayoutConfig&)> imageGenerator,
        QWidget *parent = nullptr
    );
    ~PrintDialog();

private slots:
    void handlePaintRequest(QPrinter *printer);
    void onPrinterChanged(int index);
    void onProperties();
    void onPrint();
    void onPrintOptionChanged();
    void onViewModeChanged(int index);
    void onLayoutConfigChanged();
    
    // Preview Toolbar Slots
    void onZoomChanged(int index);
    void onZoomSliderChanged(int value);
    void onPageNav(int delta);
    void onPreviewChanged(); // Updates page count/current page label

private:
    std::vector<QImage> m_images;
    QString m_configSummary;
    PrintLayoutConfig m_layoutConfig;
    std::function<std::vector<QImage>(const PrintLayoutConfig&)> m_imageGenerator;
    QPrinter *m_printer = nullptr;
    QPrintPreviewWidget *m_preview = nullptr;
    QComboBox *m_printerCombo = nullptr;
    
    // Status Labels
    QLabel *m_paperInfoLabel = nullptr;
    
    // Preview Controls
    QComboBox *m_zoomCombo = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QLabel *m_zoomValueLabel = nullptr;
    QLabel *m_pageLabel = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QComboBox *m_viewModeCombo = nullptr;

    // Modern print options
    QSpinBox *m_copiesSpin = nullptr;
    QCheckBox *m_actualSizeCheck = nullptr;
    QCheckBox *m_centerOnPageCheck = nullptr;
    QComboBox *m_orientationCombo = nullptr;
    QComboBox *m_colorModeCombo = nullptr;
    QComboBox *m_qualityCombo = nullptr;
    QLabel *m_configInfoLabel = nullptr;
    QLabel *m_jobSummaryLabel = nullptr;

    // Layout override values (matches README config order)
    QSpinBox *m_tlSpin = nullptr;
    QSpinBox *m_tsSpin = nullptr;
    QSpinBox *m_psSpin = nullptr;
    QSpinBox *m_txSpin = nullptr;
    QSpinBox *m_tySpin = nullptr;
    QSpinBox *m_pxSpin = nullptr;
    QSpinBox *m_pySpin = nullptr;
    QSpinBox *m_stxSpin = nullptr;
    QSpinBox *m_stySpin = nullptr;
    QSpinBox *m_xoSpin = nullptr;

    void setupUi();
    void applyPrinterOptions();
    void syncOptionControlsFromPrinter();
    PrintLayoutConfig readLayoutFromControls() const;
    void updateJobSummary();
    void updatePrinterInfo();
    void refreshPreview();
};

#endif
