#pragma once

#include <QDialog>

class QWidget;
class QCheckBox;
class QSpinBox;

class VTSettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit VTSettingsDialog(QWidget *parent);

	/* Shows the single shared instance, creating it on first use. */
	static void show_dialog();
	static void shutdown();

private:
	struct CodecControls {
		const char *codec = nullptr;
		QCheckBox *realtime = nullptr;
		QCheckBox *prioritize_speed = nullptr;
		QCheckBox *lookahead_enabled = nullptr;
		QSpinBox *lookahead_frames = nullptr;
	};

	CodecControls h264_controls;
	CodecControls hevc_controls;
	QCheckBox *multitrack_enabled = nullptr;

	QWidget *build_codec_section(const char *codec, const char *title_key, CodecControls &controls);
	void refresh_from_config();
};
