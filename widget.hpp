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
  INTERVAL_TIME = 0,
  NUM_OF_TRIALS,
  LIQUID_JUNCT_POTENTIAL,
  TRIAL,
  SEGMENT,
  SWEEP,
  TIME,
  VOLTAGE_OUT
};

inline std::vector<Widgets::Variable::Info> get_default_vars()
{
  return {{INTERVAL_TIME,
           "Interval Time",
           "Time allocated between intervals",
           Widgets::Variable::DOUBLE_PARAMETER,
           0.0},
          {NUM_OF_TRIALS,
           "Number of Trials",
           "Number of times to apply the loaded protocol",
           Widgets::Variable::INT_PARAMETER,
           int64_t {0}},
          {LIQUID_JUNCT_POTENTIAL,
           "Liquid Junct. Potential (mV)",
           "(mV)",
           Widgets::Variable::DOUBLE_PARAMETER,
           0.0},
          {TRIAL,
           "Trial",
           "Number of the trial currently being run",
           Widgets::Variable::STATE,
           uint64_t {0}},
          {SEGMENT,
           "Segment",
           "Number of the protocol segment being executed",
           Widgets::Variable::STATE,
           uint64_t {0}},
          {SWEEP,
           "Sweep",
           "Sweep number in current segment",
           Widgets::Variable::STATE,
           uint64_t {0}},
          {TIME,
           "Time (ms)",
           "Elapsed time for current trial",
           Widgets::Variable::STATE,
           uint64_t {0}},
          {VOLTAGE_OUT,
           "Voltage Out (V w/ LJP)",
           "Voltage output (V)",
           Widgets::Variable::STATE,
           uint64_t {0}}};
}

inline std::vector<IO::channel_t> get_default_channels()
{
  return {{
              "Current In (A)",
              "Applied current (A)",
              IO::INPUT,
          },
          {
              "Voltage Out (V w/ LJP)",
              "Voltage output with liquid junction potential",
              IO::OUTPUT,
          }};
}

struct data_token_t
{
  int64_t stepStart;
  double value;
  int trial;
  int segment;
  int sweep;
  int step;
};

enum ampMode_t : int
{
  VOLTAGE = 0,
  CURRENT
};

enum stepType_t : int
{
  STEP = 0,
  RAMP,
};

// DO NOT REORDER! IF ADDING MORE PARAMETERS INSERT RIGHT BEFORE
// PROTOCOL_PARAMETERS_SIZE!
enum protocol_parameters : size_t
{
  STEP_DURATION = 0,
  DELTA_STEP_DURATION,
  HOLDING_LEVEL_1,
  DELTA_HOLDING_LEVEL_1,
  HOLDING_LEVEL_2,
  DELTA_HOLDING_LEVEL_2,
  PROTOCOL_PARAMETERS_SIZE
};

// Individual step within a protocol
struct ProtocolStep
{
  ampMode_t ampMode = VOLTAGE;
  stepType_t stepType = STEP;
  std::array<double,
             static_cast<size_t>(protocol_parameters::PROTOCOL_PARAMETERS_SIZE)>
      parameters {};
};  // struct ProtocolStep

// A segment within a protocol, made up of ProtocolSteps
struct ProtocolSegment
{
  std::vector<ProtocolStep> steps;
  size_t numSweeps = 1;
};

class Protocol
{
public:
  // These should be private
  ProtocolSegment& getSegment(size_t seg_id);  // Return a segment
  size_t numSegments();  // Number of segments in a protocol
  size_t numSweeps(size_t seg_id);  // Number of sweeps in a segment
  int segmentLength(size_t seg_id,
                    double period,
                    bool withSweeps);  // Points in a segment (w/ or w/o
                                       // sweeps), length / period
  void setSweeps(size_t seg_id, uint32_t sweeps);  // Set sweeps for a segment
  ProtocolStep& getStep(size_t segement,
                        size_t step);  // Return step in a segment
  size_t numSteps(size_t segment);  // Return number of steps in segment
  void toDoc();  // Convert protocol to QDomDocument
  void fromDoc(QDomDocument);  // Load protocol from a QDomDocument
  void clear();  // Clears container
  // std::vector<std::vector<double>> run(
  //     double);  // Runs the protocol, returns a time and output vector

  void addSegment();  // Add a segment to container
  void deleteSegment(size_t seg_id);  // Delete a segment from container
  void modifySegment(size_t seg_id, const ProtocolSegment& segment);
  void addStep(size_t seg_id);  // Add a step to a segment in container
  void deleteStep(size_t seg_id,
                  size_t step_id);  // Delete a step from segment in container
  void modifyStep(size_t seg_id, size_t step_id, const ProtocolStep& step);

  QDomDocument& getProtocolDoc() { return protocolDoc; }
  std::array<std::vector<double>, 2> dryrun(double period);

private:
  QDomElement segmentToNode(QDomDocument& doc, size_t seg_id);
  QDomElement stepToNode(QDomDocument& doc, size_t seg_id, size_t stepNum);
  QDomDocument protocolDoc;
  std::vector<ProtocolSegment> segments;
};  // class Protocol

class ClampProtocolWindow : public QWidget
{
  Q_OBJECT

public:
  explicit ClampProtocolWindow(QWidget* /*, Panel * */);
  void createGUI();

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

  QHBoxLayout* frameLayout = nullptr;
  QSpacerItem* spacer = nullptr;
  QGridLayout* layout1 = nullptr;
  QVBoxLayout* layout2 = nullptr;
  QVBoxLayout* layout3 = nullptr;

  QFrame* frame = nullptr;
  QLabel* currentScaleLabel = nullptr;
  QComboBox* currentScaleEdit = nullptr;
  QSpinBox* currentY2Edit = nullptr;
  QComboBox* timeScaleEdit = nullptr;
  QSpinBox* timeX2Edit = nullptr;
  QSpinBox* currentY1Edit = nullptr;
  QLabel* timeScaleLabel = nullptr;
  QSpinBox* timeX1Edit = nullptr;
  QPushButton* setAxesButton = nullptr;
  QCheckBox* overlaySweepsCheckBox = nullptr;
  QCheckBox* plotAfterCheckBox = nullptr;
  QLabel* textLabel1 = nullptr;
  QComboBox* colorByComboBox = nullptr;
  QPushButton* clearButton = nullptr;

  QMdiSubWindow* subWindow = nullptr;

signals:
  void emitCloseSignal();

};  // class ClampProtocolWindow

class ClampProtocolEditor : public QWidget
{
  Q_OBJECT

public:
  explicit ClampProtocolEditor(QWidget* parent);
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
  void updateStepType(int, stepType_t);
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

// Offset from parameter index in step struct to panel's row index
constexpr int param_2_row_offset = 2;

class Panel : public Widgets::Panel
{
  Q_OBJECT
public:
  Panel(QMainWindow* main_window, Event::Manager* ev_manager);
  void exec();
  void initParameters();
  void customizeGUI();

  void foreignToggleProtocol(bool);

  void receiveEvent(const ::Event::Object*);
  void receiveEventRT(const ::Event::Object*);

public slots:
  void loadProtocolFile();
  void openProtocolEditor();
  void openProtocolWindow();
  void updateProtocolWindow();
  void closeProtocolWindow();
  void closeProtocolEditor();
  void toggleProtocol();

signals:
  void plotCurve(double*, data_token_t);

private:
  std::list<ClampProtocolWindow*> plotWindowList;

  double voltage, junctionPotential;
  double trial, time, sweep, segmentNumber, intervalTime;

  Protocol protocol;
  double stepOutput;
  double outputFactor, inputFactor;
  double rampIncrement;
  RT::OS::Fifo* fifo;
  std::vector<double> data;

  double prevSegmentEnd;  // Time segment ends after its first sweep
  int stepStart;  // Time when step starts divided by period

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
};

class Component : public Widgets::Component
{
public:
  explicit Component(Widgets::Plugin* hplugin);
  void execute() override;
private:
  double getProtocolAmplitude(int64_t current_time);
  int segmentIdx;
  int sweepIdx;
  int stepIdx;
  int trialIdx;
  int numTrials;
  int64_t reference_time=0;
  bool plotting=false;
  Protocol* protocol=nullptr;
  RT::OS::Fifo* fifo = nullptr;
};

class Plugin : public Widgets::Plugin
{
public:
  explicit Plugin(Event::Manager* ev_manager);
};

}  // namespace clamp_protocol
