/*
 * Copyright (C) 2011 Weill Medical College of Cornell University
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <QCheckBox>
#include <QComboBox>
#include <QDomDocument>
#include <QListWidget>
#include <QSpinBox>
#include <QTableWidget>

#include <qwt_plot_curve.h>
#include <rtxi/plot/basicplot.h>
#include <rtxi/widgets.hpp>

// This is an generated header file. You may change the namespace, but
// make sure to do the same in implementation (.cpp) file
namespace clamp_protocol
{

constexpr std::string_view MODULE_NAME = "clamp-protocol";

enum PARAMETER : Widgets::Variable::Id
{
  // set parameter ids here
  FIRST_PARAMETER = 0,
  SECOND_PARAMETER,
  THIRD_PARAMETER
};

inline std::vector<Widgets::Variable::Info> get_default_vars()
{
  return {{PARAMETER::FIRST_PARAMETER,
           "First Parameter Name",
           "First Parameter Description",
           Widgets::Variable::INT_PARAMETER,
           int64_t {0}},
          {PARAMETER::SECOND_PARAMETER,
           "Second Parameter Name",
           "Second Parameter Description",
           Widgets::Variable::DOUBLE_PARAMETER,
           1.0},
          {PARAMETER::THIRD_PARAMETER,
           "Third Parameter Name",
           "Third Parameter Description",
           Widgets::Variable::UINT_PARAMETER,
           uint64_t {1}}};
}

inline std::vector<IO::channel_t> get_default_channels()
{
  return {{"First Channel Output Name",
           "First Channel Output Description",
           IO::OUTPUT},
          {"First Channel Input Name",
           "First Channel Input Description",
           IO::INPUT}};
}

struct curve_token_t
{  // Token used in fifo, holds size of curve
  int trial;
  int sweep;
  bool lastStep;
  double period;  // Time period while taking data
  size_t points;
  int stepStart;  // Actual time sweep started divided by period, used in normal
                  // plotting
  int stepStartSweep;  // Time used to overlay sweeps, unitless
  double prevSegmentEnd;  // Time when previous segment ended if protocol had
                          // sweeps = 1 for all segments
};

class ProtocolStep
{  // Individual step within a protocol
public:
  ProtocolStep();
  double retrieve(int);

  enum ampMode_t
  {
    VOLTAGE,
    CURRENT
  } ampMode;
  enum stepType_t
  {
    STEP,
    RAMP,
    TRAIN,
    CURVE
  } stepType;
  double stepDuration;
  double deltaStepDuration;
  double delayBefore;
  double delayAfter;
  double holdingLevel1;
  double deltaHoldingLevel1;
  double holdingLevel2;
  double deltaHoldingLevel2;
  double pulseWidth;
  int pulseRate;
};  // class ProtocolStep

class ProtocolSegment
{  // A segment within a protocol, made up of ProtocolSteps
  friend class Protocol;

public:
  ProtocolSegment();

private:
  std::vector<ProtocolStep> segmentContainer;
  int numSweeps;
};

class Protocol
{
  friend class ClampProtocolEditor;

public:
  Protocol();

  // These should be private
  ProtocolSegment getSegment(int);  // Return a segment
  int numSegments();  // Number of segments in a protocol
  int numSweeps(int);  // Number of sweeps in a segment
  int segmentLength(
      int,
      double,
      bool);  // Points in a segment (w/ or w/o sweeps), length / period
  void setSweeps(int, int);  // Set sweeps for a segment
  ProtocolStep getStep(int, int);  // Return step in a segment
  int numSteps(int);  // Return number of steps in segment
  void toDoc();  // Convert protocol to QDomDocument
  void fromDoc(QDomDocument);  // Load protocol from a QDomDocument
  void clear();  // Clears container
  std::vector<std::vector<double>> run(
      double);  // Runs the protocol, returns a time and output vector

  std::vector<ProtocolSegment> protocolContainer;
  QDomDocument protocolDoc;

private:
  int addSegment(int);  // Add a segment to container
  int deleteSegment(int);  // Delete a segment from container
  int addStep(int, int);  // Add a step to a segment in container
  int deleteStep(int, int);  // Delete a step from segment in container

  QDomElement segmentToNode(QDomDocument&, int);
  QDomElement stepToNode(QDomDocument&, int, int);
};  // class Protocol

class ClampProtocolWindow : public QWidget
{
  Q_OBJECT

public:
  ClampProtocolWindow(QWidget* /*, Panel * */);
  void createGUI();

private:
  QHBoxLayout* frameLayout;
  QSpacerItem* spacer;
  QGridLayout* layout1;
  QVBoxLayout* layout2;
  QVBoxLayout* layout3;

public slots:
  void addCurve(double*, curve_token_t);

private slots:
  void setAxes();
  void clearPlot();
  void toggleOverlay();
  void togglePlotAfter();
  void changeColorScheme(int);

private:
  void colorCurve(QwtPlotCurve*, int);
  void closeEvent(QCloseEvent*);

  BasicPlot* plot;
  std::vector<QwtPlotCurve*>
      curveContainer;  // Used to hold curves to control memory allocation and
                       // deallocation
  bool overlaySweeps;  // True: sweeps are plotted on same time scale
  bool plotAfter;  // True: only replot after a protocol has ended, False:
                   // replot after each step
  int colorScheme;  // 0: color by run, 1: color by trial, 2: color by sweep
  int runCounter;  // Used in run color scheme
  int sweepsShown;  // Used to keep track of sweeps shown in legend
  QFont font;

  QPixmap image0;
  QPixmap image1;

  QFrame* frame;
  QLabel* currentScaleLabel;
  QComboBox* currentScaleEdit;
  QSpinBox* currentY2Edit;
  QComboBox* timeScaleEdit;
  QSpinBox* timeX2Edit;
  QSpinBox* currentY1Edit;
  QLabel* timeScaleLabel;
  QSpinBox* timeX1Edit;
  QPushButton* setAxesButton;
  QCheckBox* overlaySweepsCheckBox;
  QCheckBox* plotAfterCheckBox;
  QLabel* textLabel1;
  QComboBox* colorByComboBox;
  QPushButton* clearButton;

  QMdiSubWindow* subWindow;

signals:
  void emitCloseSignal();

};  // class ClampProtocolWindow

class ClampProtocolEditor : public QWidget
{
  Q_OBJECT

public:
  ClampProtocolEditor(QWidget*);
  void createGUI();

public slots:
  QString loadProtocol();
  void loadProtocol(QString);
  void clearProtocol();
  void exportProtocol();
  void previewProtocol();
  void comboBoxChanged(QString);
  virtual void protocolTable_currentChanged(int, int);
  virtual void protocolTable_verticalSliderReleased();

private slots:
  void addSegment();
  void deleteSegment();
  void addStep();
  void insertStep();
  void deleteStep();
  void updateSegment(QListWidgetItem*);
  void updateSegmentSweeps(int);
  void updateTableLabel();
  void updateTable();
  void updateStepAttribute(int, int);
  void updateStepType(int, ProtocolStep::stepType_t);
  void saveProtocol();

private:
  void createStep(int);
  int loadFileToProtocol(QString);
  bool protocolEmpty();
  void closeEvent(QCloseEvent*);

  Protocol protocol;  // Clamp protocol
  QPushButton *saveProtocolButton, *loadProtocolButton, *exportProtocolButton,
      *previewProtocolButton, *clearProtocolButton;
  QGroupBox* protocolDescriptionBox;
  QLabel* segmentStepLabel;
  QTableWidget* protocolTable;
  QPushButton *addStepButton, *insertStepButton, *deleteStepButton;
  QGroupBox *segmentSummaryGroup, *segmentSweepGroup;
  QLabel* segmentSweepLabel;
  QSpinBox* segmentSweepSpinBox;
  QListWidget* segmentListWidget;
  QPushButton *addSegmentButton, *deleteSegmentButton;

  QMdiSubWindow* subWindow;

  int currentSegmentNumber;
  QStringList ampModeList, stepTypeList;
  
  QHBoxLayout *layout1, *layout4, *segmentSweepGroupLayout;
  QVBoxLayout *windowLayout, *layout3, *protocolDescriptionBoxLayout, *layout5,
      *segmentSummaryGroupLayout, *layout6;
  QGridLayout* layout2;

signals:
  void protocolTableScroll();
  void emitCloseSignal();
};

class Panel : public Widgets::Panel
{
  Q_OBJECT
public:
  Panel(QMainWindow* main_window, Event::Manager* ev_manager);
  void initParameters();
  void customizeGUI();

  void foreignToggleProtocol(bool);

  void receiveEvent(const ::Event::Object*);
  void receiveEventRT(const ::Event::Object*);

  std::list<ClampProtocolWindow*> plotWindowList;

  //		QString fileName;
  double period;
  double voltage, junctionPotential;
  double trial, time, sweep, segmentNumber, intervalTime;
  int numTrials;

  Protocol protocol;
  enum executeMode_t
  {
    IDLE,
    PROTOCOL
  } executeMode;
  enum protocolMode_t
  {
    SEGMENT,
    STEP,
    EXECUTE,
    END,
    WAIT
  } protocolMode;
  ProtocolStep step;
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
  RT::OS::Fifo* fifo;
  std::vector<double> data;

  double prevSegmentEnd;  // Time segment ends after its first sweep
  int stepStart;  // Time when step starts divided by period
  curve_token_t token;

  bool recordData;
  bool protocolOn;
  bool recording;
  bool plotting;
  QTimer* plotTimer;

  QCheckBox* recordCheckBox;
  QLineEdit* loadFilePath;
  QPushButton *loadButton, *editorButton, *viewerButton, *runProtocolButton;
  ClampProtocolWindow* plotWindow;
  ClampProtocolEditor* protocolEditor;

  // friend class ToggleProtocolEvent;
  // class ToggleProtocolEvent : public RT::Event
  //{
  // public:
  //   ToggleProtocolEvent(ClampProtocol*, bool, bool);
  //   ~ToggleProtocolEvent();
  //   int callback();

  // private:
  //   ClampProtocol* parent;
  //   bool protocolOn;
  //   bool recordData;
  // };

signals:
  void plotCurve(double*, curve_token_t);

public slots:
  void loadProtocolFile();
  void openProtocolEditor();
  void openProtocolWindow();
  void updateProtocolWindow();
  void closeProtocolWindow();
  void closeProtocolEditor();
  void toggleProtocol();
  // Any functions and data related to the GUI are to be placed here
};

class Component : public Widgets::Component
{
public:
  explicit Component(Widgets::Plugin* hplugin);
  void execute() override;

  // Additional functionality needed for RealTime computation is to be placed
  // here
};

class Plugin : public Widgets::Plugin
{
public:
  explicit Plugin(Event::Manager* ev_manager);
};

}  // namespace clamp_protocol
