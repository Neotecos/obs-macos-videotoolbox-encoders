#include "settings-dialog.hpp"
#include "plugin-config.h"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPointer>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QPointer<VTSettingsDialog> g_dialog;

constexpr int kBlockSpacing = 4;
constexpr int kGapAfterBlock = 10;
constexpr int kTooltipWidthPx = 300;

bool lookahead_available()
{
	if (__builtin_available(macOS 15.0, *)) {
		return true;
	}
	return false;
}

/* Caps the tooltip's line width instead of letting a long explanation render
   as one very wide line; beyond this width it wraps and grows vertically. */
QString wrap_tooltip(const QString &text)
{
	return QStringLiteral("<div style='width:%1px;'>%2</div>").arg(kTooltipWidthPx).arg(text.toHtmlEscaped());
}

/* How far a checkbox's own label starts, so text below it can line up with
   that label instead of with the checkbox's indicator square. */
int checkbox_text_indent(const QCheckBox *checkbox)
{
	return checkbox->style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, checkbox) +
	       checkbox->style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, nullptr, checkbox);
}

/* One muted, italic summary line ending in a small "(?)" that carries the
   detailed tooltip - the tooltip trigger is that marker, not the setting's
   own name. */
QLabel *make_summary_with_help(const QString &summary, const QString &tooltip_html)
{
	auto *label = new QLabel(
		QStringLiteral("<i>%1</i>&nbsp;<span style='font-style:normal;'>(?)</span>").arg(summary.toHtmlEscaped()));
	label->setWordWrap(true);
	label->setToolTip(tooltip_html);
	label->setCursor(Qt::WhatsThisCursor);

	QFont font = label->font();
	font.setPointSizeF(font.pointSizeF() * 0.92);
	label->setFont(font);

	QPalette pal = label->palette();
	pal.setColor(QPalette::WindowText, pal.color(QPalette::PlaceholderText));
	label->setPalette(pal);

	return label;
}

QCheckBox *add_toggle_row(QVBoxLayout *section_layout, const char *label_key, const char *summary_key,
			   const char *tooltip_key)
{
	auto *checkbox = new QCheckBox(obs_module_text(label_key));
	section_layout->addWidget(checkbox);

	auto *summary =
		make_summary_with_help(obs_module_text(summary_key), wrap_tooltip(obs_module_text(tooltip_key)));
	summary->setContentsMargins(checkbox_text_indent(checkbox), 0, 0, 0);
	section_layout->addWidget(summary);
	section_layout->addSpacing(kGapAfterBlock);

	return checkbox;
}

} // namespace

QWidget *VTSettingsDialog::build_codec_section(const char *codec, const char *title_key, CodecControls &controls)
{
	controls.codec = codec;

	auto *group = new QGroupBox(obs_module_text(title_key));
	auto *layout = new QVBoxLayout(group);
	layout->setSpacing(kBlockSpacing);

	controls.realtime = add_toggle_row(layout, "SettingsWindow.RealTime", "SettingsWindow.RealTime.Summary",
					    "SettingsWindow.RealTime.Tooltip");
	connect(controls.realtime, &QCheckBox::toggled,
		[codec](bool checked) { plugin_config_set_realtime(codec, checked); });

	controls.prioritize_speed =
		add_toggle_row(layout, "SettingsWindow.PrioritizeSpeed", "SettingsWindow.PrioritizeSpeed.Summary",
			       "SettingsWindow.PrioritizeSpeed.Tooltip");
	connect(controls.prioritize_speed, &QCheckBox::toggled,
		[codec](bool checked) { plugin_config_set_prioritize_speed(codec, checked); });

	auto *lookahead_row = new QHBoxLayout();
	lookahead_row->setSpacing(8);
	controls.lookahead_enabled = new QCheckBox(obs_module_text("SettingsWindow.Lookahead"));
	controls.lookahead_frames = new QSpinBox();
	controls.lookahead_frames->setRange(plugin_config_lookahead_min(), plugin_config_lookahead_max());
	controls.lookahead_frames->setSuffix(" f");
	lookahead_row->addWidget(controls.lookahead_enabled);
	lookahead_row->addWidget(controls.lookahead_frames);
	lookahead_row->addStretch(1);
	layout->addLayout(lookahead_row);

	const bool available = lookahead_available();
	const char *lookahead_tooltip_key =
		available ? "SettingsWindow.Lookahead.Tooltip" : "SettingsWindow.Lookahead.UnavailableTooltip";
	auto *lookahead_summary = make_summary_with_help(obs_module_text("SettingsWindow.Lookahead.Summary"),
							  wrap_tooltip(obs_module_text(lookahead_tooltip_key)));
	lookahead_summary->setContentsMargins(checkbox_text_indent(controls.lookahead_enabled), 0, 0, 0);
	layout->addWidget(lookahead_summary);
	layout->addSpacing(kGapAfterBlock);

	if (!available) {
		controls.lookahead_enabled->setEnabled(false);
		controls.lookahead_frames->setEnabled(false);
	}

	QSpinBox *lookahead_frames = controls.lookahead_frames;
	connect(controls.lookahead_enabled, &QCheckBox::toggled, [codec, lookahead_frames](bool checked) {
		lookahead_frames->setEnabled(checked);
		if (checked) {
			/* Always starts from the default value when the user turns it on. */
			lookahead_frames->setValue(plugin_config_lookahead_default());
		}
		plugin_config_set_lookahead(codec, checked, lookahead_frames->value());
	});
	QCheckBox *lookahead_enabled = controls.lookahead_enabled;
	connect(controls.lookahead_frames, &QSpinBox::editingFinished, [codec, lookahead_enabled, lookahead_frames]() {
		plugin_config_set_lookahead(codec, lookahead_enabled->isChecked(), lookahead_frames->value());
	});

	return group;
}

VTSettingsDialog::VTSettingsDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(obs_module_text("SettingsWindow.Title"));

	auto *main_layout = new QVBoxLayout(this);
	main_layout->setSpacing(14);
	main_layout->setContentsMargins(16, 16, 16, 16);

	auto *multitrack_group = new QGroupBox(obs_module_text("SettingsWindow.SectionMultitrack"));
	auto *multitrack_layout = new QVBoxLayout(multitrack_group);
	multitrack_layout->setSpacing(kBlockSpacing);
	multitrack_enabled = add_toggle_row(multitrack_layout, "SettingsWindow.Multitrack",
					     "SettingsWindow.Multitrack.Summary", "SettingsWindow.Multitrack.Tooltip");
	connect(multitrack_enabled, &QCheckBox::toggled,
		[](bool checked) { plugin_config_set_multitrack_enabled(checked); });
	main_layout->addWidget(multitrack_group);

	/* Both codec sections share one frame so they read as one cluster of
	   related settings, while each keeps its own titled group box. */
	auto *encoders_frame = new QFrame();
	encoders_frame->setFrameShape(QFrame::StyledPanel);
	auto *encoders_layout = new QVBoxLayout(encoders_frame);
	encoders_layout->setSpacing(8);
	encoders_layout->addWidget(build_codec_section("h264", "SettingsWindow.SectionH264", h264_controls));
	encoders_layout->addWidget(build_codec_section("hevc", "SettingsWindow.SectionHEVC", hevc_controls));
	main_layout->addWidget(encoders_frame);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::close);
	main_layout->addWidget(buttons);

	setMinimumWidth(440);

	refresh_from_config();
}

void VTSettingsDialog::refresh_from_config()
{
	for (CodecControls *controls : {&h264_controls, &hevc_controls}) {
		vt_codec_tuning_t tuning;
		plugin_config_get_tuning(controls->codec, &tuning);

		const QSignalBlocker realtime_blocker(controls->realtime);
		controls->realtime->setChecked(tuning.realtime);

		const QSignalBlocker prioritize_blocker(controls->prioritize_speed);
		controls->prioritize_speed->setChecked(tuning.prioritize_speed);

		const QSignalBlocker lookahead_enabled_blocker(controls->lookahead_enabled);
		const QSignalBlocker lookahead_frames_blocker(controls->lookahead_frames);
		controls->lookahead_frames->setValue(tuning.lookahead_frames);
		controls->lookahead_enabled->setChecked(tuning.lookahead_enabled && controls->lookahead_enabled->isEnabled());
		controls->lookahead_frames->setEnabled(controls->lookahead_enabled->isChecked());
	}

	const QSignalBlocker multitrack_blocker(multitrack_enabled);
	multitrack_enabled->setChecked(plugin_config_get_multitrack_enabled());
}

void VTSettingsDialog::show_dialog()
{
	if (!g_dialog) {
		g_dialog = new VTSettingsDialog(static_cast<QWidget *>(obs_frontend_get_main_window()));
		g_dialog->setAttribute(Qt::WA_DeleteOnClose);
	}

	g_dialog->refresh_from_config();
	g_dialog->show();
	g_dialog->raise();
	g_dialog->activateWindow();
}

void VTSettingsDialog::shutdown()
{
	if (g_dialog) {
		g_dialog->close();
	}
}
