#pragma once

#include <QtGui>
//#include <string>
#include <fifo.h>
#include <default_gui_model.h>
#include "clamp-protocol-editor.h"
#include "clamp-protocol-window.h"
#include "protocol.h"

class ClampProtocol : public DefaultGUIModel {

	Q_OBJECT

	public:

		ClampProtocol(void);
		~ClampProtocol(void);

		void initParameters(void);
		void customizeGUI(void);
		void execute(void);
//		void refresh(void);

	protected:
		virtual void update(DefaultGUIModel::update_flags_t);

	private:
//		std::list< ClampProtocolWindow * > plotWindowList;

		QString protocol_file;
		double period;
		double voltage, junctionPotential;
		double trial, time, sweep, segmentNumber, intervalTime;
		int numTrials;

		Protocol protocol;
		enum executeMode_t { IDLE, PROTOCOL } executeMode;
		enum protocolMode_t { SEGMENT, STEP, EXECUTE, END, WAIT } protocolMode;
		Step step;
		ProtocolStep::stepType_t stepType;
		int segmentIdx;
		int sweepIdx;
		int stepIdx;
		int trialIdx;
		int numSweeps;
		int numSteps;
		int stepTime, stepEndTime;
		double protocolEndTime;
		double stepOutput;
		double outputFactor, inputFactor;
		double rampIncrement;
		double pulseWidth;
		int pulseRate;
//		Fifo fifo;
		std::vector<double> data;
		
		double prevSegmentEnd; // Time segment ends after its first sweep
		int stepStart; //Time when step starts divided by period
//		curve_token_t token;

		bool recordData;
		bool protocolOn;
		bool recording;
		bool plotting;
		QTimer *plotTimer;

		QPushButton *loadButton, *editorButton, *viewerButton, *runProtocolButton;
		QCheckBox *recordCheckBox;
		QLineEdit *loadFilePath;
	
//	public signals:
//		void plotCurve( double *, curve_token_t );

	public slots:
		void loadProtocolFile(void);
		void openProtocolEditor(void);
		void openProtocolViewer(void);
		void toggleProtocol(void);
};
