#include "../include/PrintDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QPrinterInfo>
#include <QPainter>
#include <QPrintDialog>
#include <QPageSetupDialog>
#include <QFrame>
#include <QPageLayout>
#include <QPageSize>
#include <QSpinBox>
#include <QCheckBox>
#include <QSlider>
#include <QScrollArea>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QSettings>
#include <QSizePolicy>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>

PrintDialog::PrintDialog(
	const std::vector<QImage>& images,
	const QString &configSummary,
	const PrintLayoutConfig &initialLayout,
	std::function<std::vector<QImage>(const PrintLayoutConfig&)> imageGenerator,
	QWidget *parent)
	: QDialog(parent), m_images(images), m_configSummary(configSummary), m_layoutConfig(initialLayout), m_imageGenerator(imageGenerator)
{
	m_printer = new QPrinter(QPrinter::ScreenResolution);

	setupUi();

	connect(m_preview, &QPrintPreviewWidget::paintRequested, this, &PrintDialog::handlePaintRequest);
	connect(m_preview, &QPrintPreviewWidget::previewChanged, this, &PrintDialog::onPreviewChanged);
	setWindowTitle("Print Labels");
	QScreen *targetScreen = parentWidget() ? parentWidget()->screen() : QGuiApplication::primaryScreen();
	if (targetScreen) {
		const QRect available = targetScreen->availableGeometry();
		const int margin = 16;
		const QSize maxAllowed(qMax(640, available.width() - margin * 2),
		                      qMax(520, available.height() - margin * 2));
		setMaximumSize(maxAllowed);
		resize(qMin(1200, maxAllowed.width()), qMin(800, maxAllowed.height()));
	} else {
		resize(1200, 800);
	}
	setSizeGripEnabled(true);
	loadDialogSettings();

	QTimer::singleShot(0, this, [this]() {
		updatePrinterInfo();
		updateJobSummary();
		refreshPreview();
	});
}

PrintDialog::~PrintDialog() {
	delete m_printer;
}

void PrintDialog::setupUi() {
	QHBoxLayout *mainLayout = new QHBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);

	QScrollArea *sidebarScroll = new QScrollArea(this);
	sidebarScroll->setObjectName("pd_SidebarScroll");
	sidebarScroll->setWidgetResizable(true);
	sidebarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	sidebarScroll->setFrameShape(QFrame::NoFrame);
	sidebarScroll->setMinimumWidth(300);
	sidebarScroll->setMaximumWidth(360);

	QFrame *sidebar = new QFrame();
	sidebar->setObjectName("pd_Sidebar");
	QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebar);
	sidebarLayout->setContentsMargins(14, 14, 14, 14);
	sidebarLayout->setSpacing(8);

	QLabel *titleLabel = new QLabel("Print Labels", sidebar);
	titleLabel->setObjectName("pd_Title");
	sidebarLayout->addWidget(titleLabel);

	QLabel *subtitleLabel = new QLabel("Professional print layout and device settings", sidebar);
	subtitleLabel->setObjectName("pd_Subtitle");
	subtitleLabel->setWordWrap(true);
	sidebarLayout->addWidget(subtitleLabel);

	auto makeHeader = [](const QString &text) {
		QLabel *l = new QLabel(text);
		l->setObjectName("pd_Header");
		QFont f = l->font();
		f.setBold(true);
		l->setFont(f);
		return l;
	};

	sidebarLayout->addWidget(makeHeader("Printer"));
	QFrame *printerCard = new QFrame(sidebar);
	printerCard->setObjectName("pd_Card");
	QVBoxLayout *printerCardLayout = new QVBoxLayout(printerCard);
	printerCardLayout->setContentsMargins(10, 10, 10, 10);
	printerCardLayout->setSpacing(6);

	m_printerCombo = new QComboBox(sidebar);
	m_printerCombo->setObjectName("pd_Combo");
	const QString currentPrinter = m_printer->printerName().trimmed();
	if (currentPrinter.isEmpty()) {
		m_printerCombo->addItem("System default");
	} else {
		m_printerCombo->addItem(currentPrinter);
	}
	connect(m_printerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PrintDialog::onPrinterChanged);
	printerCardLayout->addWidget(m_printerCombo);
	sidebarLayout->addWidget(printerCard);

	sidebarLayout->addWidget(makeHeader("Print Options"));
	QFrame *optionsCard = new QFrame(sidebar);
	optionsCard->setObjectName("pd_Card");
	QVBoxLayout *optionsCardLayout = new QVBoxLayout(optionsCard);
	optionsCardLayout->setContentsMargins(10, 10, 10, 10);
	optionsCardLayout->setSpacing(6);

	m_copiesSpin = new QSpinBox(sidebar);
	m_copiesSpin->setRange(1, 999);
	m_copiesSpin->setValue(1);
	QFormLayout *optionsForm = new QFormLayout();
	optionsForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	optionsForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
	optionsForm->setHorizontalSpacing(10);
	optionsForm->setVerticalSpacing(6);
	optionsForm->addRow("Copies", m_copiesSpin);

	QLabel *zoomBanner = new QLabel("Preview Zoom", sidebar);
	zoomBanner->setObjectName("pd_ZoomBanner");
	optionsCardLayout->addWidget(zoomBanner);

	QWidget *zoomRow = new QWidget(sidebar);
	QHBoxLayout *zoomRowLayout = new QHBoxLayout(zoomRow);
	zoomRowLayout->setContentsMargins(0, 0, 0, 0);
	zoomRowLayout->setSpacing(8);
	m_zoomSlider = new QSlider(Qt::Horizontal, sidebar);
	m_zoomSlider->setObjectName("pd_ZoomSlider");
	m_zoomSlider->setRange(25, 300);
	m_zoomSlider->setValue(100);
	m_zoomSlider->setMinimumWidth(180);
	m_zoomSlider->setFixedHeight(24);
	m_zoomSlider->setTickPosition(QSlider::TicksBelow);
	m_zoomSlider->setTickInterval(25);
	m_zoomSlider->setToolTip("Preview zoom");
	m_zoomSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	connect(m_zoomSlider, &QSlider::valueChanged, this, &PrintDialog::onZoomSliderChanged);
	zoomRowLayout->addWidget(m_zoomSlider);

	m_zoomValueLabel = new QLabel("100%", sidebar);
	m_zoomValueLabel->setObjectName("pd_ZoomValue");
	m_zoomValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	m_zoomValueLabel->setFixedWidth(44);
	zoomRowLayout->addWidget(m_zoomValueLabel);

	optionsCardLayout->addWidget(zoomRow);
	optionsCardLayout->addLayout(optionsForm);

	m_actualSizeCheck = new QCheckBox("Print at source size (300 DPI)", sidebar);
	m_actualSizeCheck->setChecked(true);
	optionsCardLayout->addWidget(m_actualSizeCheck);

	m_centerOnPageCheck = new QCheckBox("Center on page", sidebar);
	m_centerOnPageCheck->setChecked(true);
	optionsCardLayout->addWidget(m_centerOnPageCheck);

	sidebarLayout->addWidget(optionsCard);

	sidebarLayout->addWidget(makeHeader("Advanced"));
	QPushButton *advancedToggleBtn = new QPushButton("Show advanced settings", sidebar);
	advancedToggleBtn->setObjectName("pd_AdvancedToggle");
	advancedToggleBtn->setCheckable(true);
	sidebarLayout->addWidget(advancedToggleBtn);

	QFrame *advancedCard = new QFrame(sidebar);
	advancedCard->setObjectName("pd_Card");
	QVBoxLayout *advancedCardLayout = new QVBoxLayout(advancedCard);
	advancedCardLayout->setContentsMargins(10, 10, 10, 10);
	advancedCardLayout->setSpacing(6);

	m_orientationCombo = new QComboBox(sidebar);
	m_orientationCombo->addItems({"Portrait", "Landscape"});
	m_colorModeCombo = new QComboBox(sidebar);
	m_colorModeCombo->addItems({"Color", "Grayscale"});
	m_qualityCombo = new QComboBox(sidebar);
	m_qualityCombo->addItems({"High", "Normal", "Draft"});

	QFormLayout *advancedForm = new QFormLayout();
	advancedForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	advancedForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
	advancedForm->setHorizontalSpacing(10);
	advancedForm->setVerticalSpacing(6);
	advancedForm->addRow("Orientation", m_orientationCombo);
	advancedForm->addRow("Color", m_colorModeCombo);
	advancedForm->addRow("Quality", m_qualityCombo);
	advancedCardLayout->addLayout(advancedForm);

	QPushButton *propsBtn = new QPushButton("Printer properties...", sidebar);
	connect(propsBtn, &QPushButton::clicked, this, &PrintDialog::onProperties);
	advancedCardLayout->addWidget(propsBtn);

	QPushButton *pageSetupBtn = new QPushButton("Page setup", sidebar);
	connect(pageSetupBtn, &QPushButton::clicked, [this]() {
		QPageSetupDialog dialog(m_printer, this);
		if (dialog.exec() == QDialog::Accepted) {
			syncOptionControlsFromPrinter();
			saveDialogSettings();
			refreshPreview();
		}
	});
	advancedCardLayout->addWidget(pageSetupBtn);

	advancedCard->setVisible(false);
	connect(advancedToggleBtn, &QPushButton::toggled, this, [advancedCard, advancedToggleBtn](bool on) {
		advancedCard->setVisible(on);
		advancedToggleBtn->setText(on ? "Hide advanced settings" : "Show advanced settings");
	});
	sidebarLayout->addWidget(advancedCard);

	connect(m_copiesSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
		saveDialogSettings();
		refreshPreview();
	});
	connect(m_actualSizeCheck, &QCheckBox::toggled, this, [this]() {
		saveDialogSettings();
		refreshPreview();
	});
	connect(m_centerOnPageCheck, &QCheckBox::toggled, this, [this]() {
		saveDialogSettings();
		refreshPreview();
	});
	connect(m_orientationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PrintDialog::onPrintOptionChanged);
	connect(m_colorModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PrintDialog::onPrintOptionChanged);
	connect(m_qualityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PrintDialog::onPrintOptionChanged);

	sidebarLayout->addWidget(makeHeader("Settings"));
	QFrame *settingsCard = new QFrame(sidebar);
	settingsCard->setObjectName("pd_Card");
	QVBoxLayout *settingsCardLayout = new QVBoxLayout(settingsCard);
	settingsCardLayout->setContentsMargins(10, 10, 10, 10);
	settingsCardLayout->setSpacing(6);
	m_paperInfoLabel = new QLabel(sidebar);
	m_paperInfoLabel->setObjectName("pd_InfoLabel");
	m_paperInfoLabel->setWordWrap(true);
	settingsCardLayout->addWidget(m_paperInfoLabel);
	sidebarLayout->addWidget(settingsCard);

	sidebarLayout->addWidget(makeHeader("Job Summary"));
	QFrame *summaryCard = new QFrame(sidebar);
	summaryCard->setObjectName("pd_Card");
	QVBoxLayout *summaryCardLayout = new QVBoxLayout(summaryCard);
	summaryCardLayout->setContentsMargins(10, 10, 10, 10);
	summaryCardLayout->setSpacing(6);
	m_jobSummaryLabel = new QLabel(sidebar);
	m_jobSummaryLabel->setObjectName("pd_InfoLabel");
	m_jobSummaryLabel->setWordWrap(true);
	summaryCardLayout->addWidget(m_jobSummaryLabel);
	sidebarLayout->addWidget(summaryCard);

	sidebarLayout->addWidget(makeHeader("Config Defaults"));
	QFrame *configCard = new QFrame(sidebar);
	configCard->setObjectName("pd_Card");
	QVBoxLayout *configCardLayout = new QVBoxLayout(configCard);
	configCardLayout->setContentsMargins(10, 10, 10, 10);
	configCardLayout->setSpacing(6);
	m_configInfoLabel = new QLabel(sidebar);
	m_configInfoLabel->setObjectName("pd_InfoLabel");
	m_configInfoLabel->setWordWrap(true);
	m_configInfoLabel->setText(m_configSummary);
	configCardLayout->addWidget(m_configInfoLabel);
	sidebarLayout->addWidget(configCard);

	sidebarLayout->addWidget(makeHeader("Label Layout Overrides"));
	QFrame *layoutCard = new QFrame(sidebar);
	layoutCard->setObjectName("pd_Card");
	QVBoxLayout *layoutCardLayout = new QVBoxLayout(layoutCard);
	layoutCardLayout->setContentsMargins(10, 10, 10, 10);
	layoutCardLayout->setSpacing(6);

	auto makeLayoutSpin = [&](int value, int minV, int maxV) {
		QSpinBox *spin = new QSpinBox(sidebar);
		spin->setRange(minV, maxV);
		spin->setValue(value);
		return spin;
	};

	m_tlSpin = makeLayoutSpin(m_layoutConfig.TL, 1, 128);
	m_tsSpin = makeLayoutSpin(m_layoutConfig.TS, 1, 256);
	m_psSpin = makeLayoutSpin(m_layoutConfig.PS, 1, 512);
	m_txSpin = makeLayoutSpin(m_layoutConfig.TX, 0, 5000);
	m_tySpin = makeLayoutSpin(m_layoutConfig.TY, 0, 5000);
	m_pxSpin = makeLayoutSpin(m_layoutConfig.PX, 0, 5000);
	m_pySpin = makeLayoutSpin(m_layoutConfig.PY, 0, 5000);
	m_stxSpin = makeLayoutSpin(m_layoutConfig.STX, 0, 5000);
	m_stySpin = makeLayoutSpin(m_layoutConfig.STY, 0, 5000);
	m_xoSpin = makeLayoutSpin(m_layoutConfig.XO, 0, 5000);

	connect(m_tlSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrintDialog::onLayoutConfigChanged);
	connect(m_tsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrintDialog::onLayoutConfigChanged);
	connect(m_psSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrintDialog::onLayoutConfigChanged);
	connect(m_txSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrintDialog::onLayoutConfigChanged);
	connect(m_tySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrintDialog::onLayoutConfigChanged);
	connect(m_pxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrintDialog::onLayoutConfigChanged);
	connect(m_pySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrintDialog::onLayoutConfigChanged);
	connect(m_stxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrintDialog::onLayoutConfigChanged);
	connect(m_stySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrintDialog::onLayoutConfigChanged);
	connect(m_xoSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrintDialog::onLayoutConfigChanged);

	QFormLayout *layoutForm = new QFormLayout();
	layoutForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	layoutForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
	layoutForm->setHorizontalSpacing(10);
	layoutForm->setVerticalSpacing(6);
	layoutForm->addRow("Text length (TL)", m_tlSpin);
	layoutForm->addRow("Text size (TS)", m_tsSpin);
	layoutForm->addRow("Price size (PS)", m_psSpin);
	layoutForm->addRow("Text X (TX)", m_txSpin);
	layoutForm->addRow("Text Y (TY)", m_tySpin);
	layoutForm->addRow("Price X (PX)", m_pxSpin);
	layoutForm->addRow("Price Y (PY)", m_pySpin);
	layoutForm->addRow("Subtext X (STX)", m_stxSpin);
	layoutForm->addRow("Subtext Y (STY)", m_stySpin);
	layoutForm->addRow("X Offset (XO)", m_xoSpin);
	layoutCardLayout->addLayout(layoutForm);

	QLabel *layoutHelp = new QLabel("These overrides bypass manual Config.txt edits and are used when labels are generated.", sidebar);
	layoutHelp->setObjectName("pd_InfoLabel");
	layoutHelp->setWordWrap(true);
	layoutCardLayout->addWidget(layoutHelp);

	sidebarLayout->addWidget(layoutCard);

	sidebarLayout->addStretch();

	QHBoxLayout *actionLayout = new QHBoxLayout();
	QPushButton *cancelBtn = new QPushButton("Cancel", sidebar);
	cancelBtn->setObjectName("pd_CancelBtn");
	cancelBtn->setAutoDefault(false);
	connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
	QPushButton *printBtn = new QPushButton("Print", sidebar);
	printBtn->setObjectName("pd_PrintBtn");
	printBtn->setDefault(true);
	printBtn->setAutoDefault(true);
	connect(printBtn, &QPushButton::clicked, this, &PrintDialog::onPrint);
	actionLayout->addWidget(cancelBtn);
	actionLayout->addWidget(printBtn);
	sidebarLayout->addLayout(actionLayout);

	sidebarScroll->setWidget(sidebar);
	mainLayout->addWidget(sidebarScroll);

	QVBoxLayout *rightLayout = new QVBoxLayout();
	rightLayout->setContentsMargins(10, 10, 10, 10);
	rightLayout->setSpacing(8);

	QFrame *toolbarFrame = new QFrame(this);
	toolbarFrame->setObjectName("pd_Toolbar");
	toolbarFrame->setFixedHeight(44);
	QHBoxLayout *toolLayout = new QHBoxLayout(toolbarFrame);
	toolLayout->setContentsMargins(10, 6, 10, 6);
	toolLayout->addWidget(new QLabel("View:"));

	m_viewModeCombo = new QComboBox(toolbarFrame);
	m_viewModeCombo->addItems({"Single", "Facing", "All pages"});
	m_viewModeCombo->setCurrentIndex(0);
	connect(m_viewModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PrintDialog::onViewModeChanged);
	toolLayout->addWidget(m_viewModeCombo);

	toolLayout->addWidget(new QLabel("Zoom:"));

	m_zoomCombo = new QComboBox(toolbarFrame);
	m_zoomCombo->addItems({"Fit Width", "Fit Page", "50%", "75%", "100%", "150%", "200%"});
	m_zoomCombo->setCurrentIndex(0);
	m_zoomCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	m_zoomCombo->setMinimumWidth(92);
	connect(m_zoomCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PrintDialog::onZoomChanged);
	toolLayout->addWidget(m_zoomCombo);

	toolLayout->addStretch();

	m_prevBtn = new QPushButton("<", toolbarFrame);
	m_prevBtn->setFixedWidth(30);
	m_prevBtn->setShortcut(QKeySequence(Qt::Key_PageUp));
	connect(m_prevBtn, &QPushButton::clicked, [this]() { onPageNav(-1); });

	m_pageLabel = new QLabel("Page 1 / 1", toolbarFrame);
	m_pageLabel->setObjectName("pd_PageLabel");
	m_pageLabel->setAlignment(Qt::AlignCenter);
	m_pageLabel->setFixedWidth(100);

	m_nextBtn = new QPushButton(">", toolbarFrame);
	m_nextBtn->setFixedWidth(30);
	m_nextBtn->setShortcut(QKeySequence(Qt::Key_PageDown));
	connect(m_nextBtn, &QPushButton::clicked, [this]() { onPageNav(1); });

	toolLayout->addWidget(m_prevBtn);
	toolLayout->addWidget(m_pageLabel);
	toolLayout->addWidget(m_nextBtn);

	rightLayout->addWidget(toolbarFrame);

	m_preview = new QPrintPreviewWidget(m_printer, this);
	m_preview->setZoomMode(QPrintPreviewWidget::FitToWidth);
	rightLayout->addWidget(m_preview);

	mainLayout->addLayout(rightLayout);
	mainLayout->setStretch(0, 0);
	mainLayout->setStretch(1, 1);

	setTabOrder(m_printerCombo, m_copiesSpin);
	setTabOrder(m_copiesSpin, m_actualSizeCheck);
	setTabOrder(m_actualSizeCheck, m_centerOnPageCheck);
	setTabOrder(m_centerOnPageCheck, advancedToggleBtn);
	setTabOrder(advancedToggleBtn, m_orientationCombo);
	setTabOrder(m_orientationCombo, m_colorModeCombo);
	setTabOrder(m_colorModeCombo, m_qualityCombo);
	setTabOrder(m_qualityCombo, m_tlSpin);
	setTabOrder(m_tlSpin, m_tsSpin);
	setTabOrder(m_tsSpin, m_psSpin);
	setTabOrder(m_psSpin, m_txSpin);
	setTabOrder(m_txSpin, m_tySpin);
	setTabOrder(m_tySpin, m_pxSpin);
	setTabOrder(m_pxSpin, m_pySpin);
	setTabOrder(m_pySpin, m_stxSpin);
	setTabOrder(m_stxSpin, m_stySpin);
	setTabOrder(m_stySpin, m_xoSpin);
	setTabOrder(m_xoSpin, m_zoomSlider);
	setTabOrder(m_zoomSlider, m_viewModeCombo);
	setTabOrder(m_viewModeCombo, m_zoomCombo);
	setTabOrder(m_zoomCombo, m_prevBtn);
	setTabOrder(m_prevBtn, m_nextBtn);
	setTabOrder(m_nextBtn, cancelBtn);
	setTabOrder(cancelBtn, printBtn);

	m_copiesSpin->setFocus(Qt::TabFocusReason);

	applyPrinterOptions();
	updatePrinterInfo();
	updateJobSummary();
}

void PrintDialog::onPrinterChanged(int index) {
	if (index < 0) return;
	QString name = m_printerCombo->itemText(index);
	if (name != "System default") {
		m_printer->setPrinterName(name);
	}
	syncOptionControlsFromPrinter();
	applyPrinterOptions();
	saveDialogSettings();
	updatePrinterInfo();
	refreshPreview();
}

void PrintDialog::onPrintOptionChanged() {
	applyPrinterOptions();
	saveDialogSettings();
	refreshPreview();
}

PrintLayoutConfig PrintDialog::readLayoutFromControls() const {
	PrintLayoutConfig cfg{};
	cfg.TL = m_tlSpin ? m_tlSpin->value() : 24;
	cfg.TS = m_tsSpin ? m_tsSpin->value() : 44;
	cfg.PS = m_psSpin ? m_psSpin->value() : 140;
	cfg.TX = m_txSpin ? m_txSpin->value() : 15;
	cfg.TY = m_tySpin ? m_tySpin->value() : 80;
	cfg.PX = m_pxSpin ? m_pxSpin->value() : 90;
	cfg.PY = m_pySpin ? m_pySpin->value() : 350;
	cfg.STX = m_stxSpin ? m_stxSpin->value() : 15;
	cfg.STY = m_stySpin ? m_stySpin->value() : 140;
	cfg.XO = m_xoSpin ? m_xoSpin->value() : 40;
	return cfg;
}

void PrintDialog::onLayoutConfigChanged() {
	m_layoutConfig = readLayoutFromControls();
	saveDialogSettings();
	if (m_imageGenerator) {
		std::vector<QImage> regenerated = m_imageGenerator(m_layoutConfig);
		if (!regenerated.empty()) {
			m_images = std::move(regenerated);
		}
	}
	refreshPreview();
}

void PrintDialog::applyPrinterOptions() {
	if (!m_printer) return;

	QPageLayout pageLayout = m_printer->pageLayout();
	if (m_orientationCombo && m_orientationCombo->currentIndex() == 1) {
		pageLayout.setOrientation(QPageLayout::Landscape);
	} else {
		pageLayout.setOrientation(QPageLayout::Portrait);
	}
	m_printer->setPageLayout(pageLayout);

	if (m_colorModeCombo && m_colorModeCombo->currentIndex() == 1) {
		m_printer->setColorMode(QPrinter::GrayScale);
	} else {
		m_printer->setColorMode(QPrinter::Color);
	}

	if (!m_qualityCombo) return;
	switch (m_qualityCombo->currentIndex()) {
		case 0:
			m_printer->setResolution(600);
			break;
		case 2:
			m_printer->setResolution(150);
			break;
		case 1:
		default:
			m_printer->setResolution(300);
			break;
	}
}

void PrintDialog::syncOptionControlsFromPrinter() {
	if (!m_printer) return;

	QSignalBlocker blockOrientation(m_orientationCombo);
	QSignalBlocker blockColor(m_colorModeCombo);
	QSignalBlocker blockQuality(m_qualityCombo);

	QPageLayout pageLayout = m_printer->pageLayout();
	m_orientationCombo->setCurrentIndex(pageLayout.orientation() == QPageLayout::Landscape ? 1 : 0);
	m_colorModeCombo->setCurrentIndex(m_printer->colorMode() == QPrinter::GrayScale ? 1 : 0);

	const int dpi = m_printer->resolution();
	if (dpi >= 450) {
		m_qualityCombo->setCurrentIndex(0);
	} else if (dpi <= 200) {
		m_qualityCombo->setCurrentIndex(2);
	} else {
		m_qualityCombo->setCurrentIndex(1);
	}
}

void PrintDialog::updatePrinterInfo() {
	if (!m_printer) return;

	QPageLayout layout = m_printer->pageLayout();
	QString pageSizeName = layout.pageSize().name();
	QString orientation = (layout.orientation() == QPageLayout::Portrait) ? "Portrait" : "Landscape";
	QString sizeStr = QString("%1 x %2 mm")
			.arg(layout.pageSize().size(QPageSize::Millimeter).width(), 0, 'f', 1)
			.arg(layout.pageSize().size(QPageSize::Millimeter).height(), 0, 'f', 1);

	m_paperInfoLabel->setText(
		QString("Paper: %1\nDimensions: %2\nOrientation: %3\nResolution: %4 DPI")
			.arg(pageSizeName, sizeStr, orientation)
			.arg(m_printer->resolution())
	);
}

void PrintDialog::onProperties() {
	QPrintDialog dialog(m_printer, this);
	if (dialog.exec() == QDialog::Accepted) {
		syncOptionControlsFromPrinter();
		saveDialogSettings();
		updatePrinterInfo();
		refreshPreview();
	}
}

void PrintDialog::refreshPreview() {
	applyPrinterOptions();
	m_preview->updatePreview();
	updatePrinterInfo();
	updateJobSummary();
}

void PrintDialog::handlePaintRequest(QPrinter *printer) {
	if (!printer || m_images.empty()) return;

	QPainter painter(printer);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::TextAntialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	QRectF pageRect = printer->pageRect(QPrinter::DevicePixel);
	const int copies = m_copiesSpin->value();

	for (int c = 0; c < copies; ++c) {
		for (int i = 0; i < static_cast<int>(m_images.size()); ++i) {
			const QImage &image = m_images[i];
			if (image.isNull()) continue;

			QRect targetRect;
			if (m_actualSizeCheck->isChecked()) {
				const double assumedSourceDpi = 300.0;
				const int targetW = qMax(1, static_cast<int>(image.width() * printer->logicalDpiX() / assumedSourceDpi));
				const int targetH = qMax(1, static_cast<int>(image.height() * printer->logicalDpiY() / assumedSourceDpi));
				targetRect = QRect(pageRect.x(), pageRect.y(), targetW, targetH);
			} else {
				QSize scaled = image.size();
				scaled.scale(pageRect.size().toSize(), Qt::KeepAspectRatio);
				targetRect = QRect(pageRect.x(), pageRect.y(), scaled.width(), scaled.height());
			}

			if (m_centerOnPageCheck->isChecked()) {
				targetRect.moveLeft(pageRect.x() + (pageRect.width() - targetRect.width()) / 2);
				targetRect.moveTop(pageRect.y() + (pageRect.height() - targetRect.height()) / 2);
			}

			painter.drawImage(targetRect, image);

			const bool isLastPage = (c == copies - 1) && (i == static_cast<int>(m_images.size()) - 1);
			if (!isLastPage) {
				printer->newPage();
			}
		}
	}
}

void PrintDialog::onPrint() {
	applyPrinterOptions();
	QPrintDialog dialog(m_printer, this);
	if (dialog.exec() == QDialog::Accepted) {
		m_layoutConfig = readLayoutFromControls();
		if (m_imageGenerator) {
			std::vector<QImage> regenerated = m_imageGenerator(m_layoutConfig);
			if (!regenerated.empty()) {
				m_images = std::move(regenerated);
			}
		}

		saveDialogSettings();

		syncOptionControlsFromPrinter();
		applyPrinterOptions();
		handlePaintRequest(m_printer);
		accept();
	}
}

void PrintDialog::onZoomChanged(int index) {
	saveDialogSettings();
	switch(index) {
		case 0:
			m_preview->setZoomMode(QPrintPreviewWidget::FitToWidth);
			m_zoomValueLabel->setText("Auto");
			break;
		case 1:
			m_preview->setZoomMode(QPrintPreviewWidget::FitInView);
			m_zoomValueLabel->setText("Auto");
			break;
		case 2:
			m_preview->setZoomFactor(0.5);
			m_zoomSlider->setValue(50);
			break;
		case 3:
			m_preview->setZoomFactor(0.75);
			m_zoomSlider->setValue(75);
			break;
		case 4:
			m_preview->setZoomFactor(1.0);
			m_zoomSlider->setValue(100);
			break;
		case 5:
			m_preview->setZoomFactor(1.5);
			m_zoomSlider->setValue(150);
			break;
		case 6:
			m_preview->setZoomFactor(2.0);
			m_zoomSlider->setValue(200);
			break;
	}
}

void PrintDialog::onZoomSliderChanged(int value) {
	if (!m_preview) return;
	saveDialogSettings();
	m_preview->setZoomFactor(static_cast<qreal>(value) / 100.0);
	if (m_zoomValueLabel) {
		m_zoomValueLabel->setText(QString("%1%").arg(value));
	}

	QSignalBlocker block(m_zoomCombo);
	if (value == 50) m_zoomCombo->setCurrentIndex(2);
	else if (value == 75) m_zoomCombo->setCurrentIndex(3);
	else if (value == 100) m_zoomCombo->setCurrentIndex(4);
	else if (value == 150) m_zoomCombo->setCurrentIndex(5);
	else if (value == 200) m_zoomCombo->setCurrentIndex(6);
}

void PrintDialog::onViewModeChanged(int index) {
	saveDialogSettings();
	switch (index) {
		case 1:
			m_preview->setViewMode(QPrintPreviewWidget::FacingPagesView);
			break;
		case 2:
			m_preview->setViewMode(QPrintPreviewWidget::AllPagesView);
			break;
		case 0:
		default:
			m_preview->setViewMode(QPrintPreviewWidget::SinglePageView);
			break;
	}
}

void PrintDialog::onPageNav(int delta) {
	int current = m_preview->currentPage();
	int newPage = current + delta;
	if (newPage >= 1 && newPage <= m_preview->pageCount()) {
		m_preview->setCurrentPage(newPage);
	}
}

void PrintDialog::onPreviewChanged() {
	int current = m_preview->currentPage();
	int total = m_preview->pageCount();
	m_pageLabel->setText(QString("Page %1 / %2").arg(current).arg(total));

	m_prevBtn->setEnabled(current > 1);
	m_nextBtn->setEnabled(current < total);
	updateJobSummary();
}

void PrintDialog::updateJobSummary() {
	if (!m_jobSummaryLabel) return;

	const int totalPages = qMax(1, m_preview ? m_preview->pageCount() : static_cast<int>(m_images.size()));
	const int copies = m_copiesSpin ? m_copiesSpin->value() : 1;
	const QString scaleMode = (m_actualSizeCheck && m_actualSizeCheck->isChecked()) ? "Actual size" : "Fit to page";
	const QString centerMode = (m_centerOnPageCheck && m_centerOnPageCheck->isChecked()) ? "Centered" : "Top-left aligned";
	const QString printerName = (m_printer && !m_printer->printerName().isEmpty()) ? m_printer->printerName() : "System default";

	m_jobSummaryLabel->setText(
		QString("Printer: %1\nLabels: %2\nCopies: %3\nOutput pages: %4\nScale: %5\nPlacement: %6")
			.arg(printerName)
			.arg(static_cast<int>(m_images.size()))
			.arg(copies)
			.arg(totalPages * copies)
			.arg(scaleMode)
			.arg(centerMode)
	);
}

void PrintDialog::loadDialogSettings() {
	QSettings settings;

	QSignalBlocker blockCopies(m_copiesSpin);
	QSignalBlocker blockActual(m_actualSizeCheck);
	QSignalBlocker blockCenter(m_centerOnPageCheck);
	QSignalBlocker blockOrientation(m_orientationCombo);
	QSignalBlocker blockColor(m_colorModeCombo);
	QSignalBlocker blockQuality(m_qualityCombo);
	QSignalBlocker blockTl(m_tlSpin);
	QSignalBlocker blockTs(m_tsSpin);
	QSignalBlocker blockPs(m_psSpin);
	QSignalBlocker blockTx(m_txSpin);
	QSignalBlocker blockTy(m_tySpin);
	QSignalBlocker blockPx(m_pxSpin);
	QSignalBlocker blockPy(m_pySpin);
	QSignalBlocker blockStx(m_stxSpin);
	QSignalBlocker blockSty(m_stySpin);
	QSignalBlocker blockXo(m_xoSpin);
	QSignalBlocker blockView(m_viewModeCombo);
	QSignalBlocker blockZoomCombo(m_zoomCombo);
	QSignalBlocker blockZoomSlider(m_zoomSlider);

	m_copiesSpin->setValue(settings.value("printDialog/copies", m_copiesSpin->value()).toInt());
	m_actualSizeCheck->setChecked(settings.value("printDialog/actualSize", m_actualSizeCheck->isChecked()).toBool());
	m_centerOnPageCheck->setChecked(settings.value("printDialog/centerOnPage", m_centerOnPageCheck->isChecked()).toBool());
	m_orientationCombo->setCurrentIndex(settings.value("printDialog/orientation", m_orientationCombo->currentIndex()).toInt());
	m_colorModeCombo->setCurrentIndex(settings.value("printDialog/colorMode", m_colorModeCombo->currentIndex()).toInt());
	m_qualityCombo->setCurrentIndex(settings.value("printDialog/quality", m_qualityCombo->currentIndex()).toInt());
	m_viewModeCombo->setCurrentIndex(settings.value("printDialog/viewMode", m_viewModeCombo->currentIndex()).toInt());
	m_zoomCombo->setCurrentIndex(settings.value("printDialog/zoomCombo", m_zoomCombo->currentIndex()).toInt());
	m_zoomSlider->setValue(settings.value("printDialog/zoomSlider", m_zoomSlider->value()).toInt());

	m_layoutConfig.TL = settings.value("printLayout/TL", m_layoutConfig.TL).toInt();
	m_layoutConfig.TS = settings.value("printLayout/TS", m_layoutConfig.TS).toInt();
	m_layoutConfig.PS = settings.value("printLayout/PS", m_layoutConfig.PS).toInt();
	m_layoutConfig.TX = settings.value("printLayout/TX", m_layoutConfig.TX).toInt();
	m_layoutConfig.TY = settings.value("printLayout/TY", m_layoutConfig.TY).toInt();
	m_layoutConfig.PX = settings.value("printLayout/PX", m_layoutConfig.PX).toInt();
	m_layoutConfig.PY = settings.value("printLayout/PY", m_layoutConfig.PY).toInt();
	m_layoutConfig.STX = settings.value("printLayout/STX", m_layoutConfig.STX).toInt();
	m_layoutConfig.STY = settings.value("printLayout/STY", m_layoutConfig.STY).toInt();
	m_layoutConfig.XO = settings.value("printLayout/XO", m_layoutConfig.XO).toInt();

	m_tlSpin->setValue(m_layoutConfig.TL);
	m_tsSpin->setValue(m_layoutConfig.TS);
	m_psSpin->setValue(m_layoutConfig.PS);
	m_txSpin->setValue(m_layoutConfig.TX);
	m_tySpin->setValue(m_layoutConfig.TY);
	m_pxSpin->setValue(m_layoutConfig.PX);
	m_pySpin->setValue(m_layoutConfig.PY);
	m_stxSpin->setValue(m_layoutConfig.STX);
	m_stySpin->setValue(m_layoutConfig.STY);
	m_xoSpin->setValue(m_layoutConfig.XO);
}

void PrintDialog::saveDialogSettings() {
	QSettings settings;

	settings.setValue("printDialog/copies", m_copiesSpin->value());
	settings.setValue("printDialog/actualSize", m_actualSizeCheck->isChecked());
	settings.setValue("printDialog/centerOnPage", m_centerOnPageCheck->isChecked());
	settings.setValue("printDialog/orientation", m_orientationCombo->currentIndex());
	settings.setValue("printDialog/colorMode", m_colorModeCombo->currentIndex());
	settings.setValue("printDialog/quality", m_qualityCombo->currentIndex());
	settings.setValue("printDialog/viewMode", m_viewModeCombo->currentIndex());
	settings.setValue("printDialog/zoomCombo", m_zoomCombo->currentIndex());
	settings.setValue("printDialog/zoomSlider", m_zoomSlider->value());

	const PrintLayoutConfig cfg = readLayoutFromControls();
	settings.setValue("printLayout/TL", cfg.TL);
	settings.setValue("printLayout/TS", cfg.TS);
	settings.setValue("printLayout/PS", cfg.PS);
	settings.setValue("printLayout/TX", cfg.TX);
	settings.setValue("printLayout/TY", cfg.TY);
	settings.setValue("printLayout/PX", cfg.PX);
	settings.setValue("printLayout/PY", cfg.PY);
	settings.setValue("printLayout/STX", cfg.STX);
	settings.setValue("printLayout/STY", cfg.STY);
	settings.setValue("printLayout/XO", cfg.XO);
}

