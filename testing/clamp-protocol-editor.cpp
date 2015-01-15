#include "clamp-protocol-editor.h"
#include <main_window.h>
#include <iostream>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_text.h>
#include <basicplot.h>

using namespace std;

CenterAlignTableItem::CenterAlignTableItem( QTableWidget *table ): QTableWidgetItem(QTableWidgetItem::Type) {} // QTableItem subclass: used to make sure text is aligned in center 

int CenterAlignTableItem::alignment() const {
	return Qt::AlignHCenter | Qt::AlignVCenter;
}

ClampProtocolEditor::ClampProtocolEditor(QWidget * parent) : QWidget(parent) {

	setWindowTitle("Protocol Editor");
	currentSegmentNumber = 0;
	createGUI();

	// QStringList for amp mode and step type combo boxes;
	ampModeList.append("Voltage");
	ampModeList.append("Current");
	stepTypeList.append("Step");
	stepTypeList.append("Ramp");
	stepTypeList.append("Train");
	//stepTypeList += "Custom";
		
	resize( minimumSize() ); // Set window size to minimum
}

void ClampProtocolEditor::addSegment( void ) { // Adds another segment to protocol: listview, protocol container, and calls summary update
std::cout<<"addSegment called"<<std::endl;
	if( !protocol.addSegment( currentSegmentNumber ) ) { // Protocol::addSegment returns 0 if it fails
		cout << "Error - ClampProtocolEditor::addSegment() failure" << endl;
		return ;
	}
	
	QString segmentName = "Segment ";
	if( protocol.numSegments() < 10) // To help with sorting, a zero prefix is used for single digits
		segmentName += "0";
	
	segmentName.append( QString( "%1" ).arg( protocol.numSegments() ) ); // Make QString of 'Segment ' + number of segments in container
	QListWidgetItem *element = new QListWidgetItem(segmentName, segmentListWidget); // Add segment reference to listView
std::cout<<"new segment named: "<<segmentName.toStdString()<<std::endl;
	
	// Find newly inserted segment
	if( currentSegmentNumber + 1 < 10 ) {
		QList<QListWidgetItem*> elements = segmentListWidget->findItems("Segment 0" + QString::number(currentSegmentNumber+1), 0);
		if (!elements.isEmpty()) element = elements.takeFirst();
	}
	else {
		QList<QListWidgetItem*> elements = segmentListWidget->findItems("Segment " + QString::number(currentSegmentNumber+1), 0);
		if (!elements.isEmpty()) element = elements.takeFirst();
	}
	
	if( !element ) { // element = 0 if nothing was found
		element = segmentListWidget->item(segmentListWidget->count());
		segmentListWidget->setCurrentItem( element );
	}
	else
	segmentListWidget->setCurrentItem( element ); // Focus on newly created segment
	
std::cout<<"Current segment number: "<<currentSegmentNumber+1<<std::endl;

	updateSegment(element);
}

void ClampProtocolEditor::deleteSegment( void ) { // Deletes segment selected in listview: listview, protocol container, and calls summary update
	if( currentSegmentNumber == 0 ) {// If no segment exists, return and output error box
		QMessageBox::warning(this,
		"Error",
		"No segment has been created or selected.");
		return ;
	}
	
	// Message box asking for confirmation whether step should be deleted
	QString segmentString;
	segmentString.setNum(currentSegmentNumber);
	QString text = "Do you wish to delete Segment " + segmentString + "?"; // Text pointing out specific segment and step
	if(QMessageBox::question(this, "Delete Segment Confirmation", text, "Yes", "No")) return ; // Answer is no
	
	if( protocol.numSegments() == 1 ) { // If only 1 segment exists, clear protocol
		protocol.clear();
	}
	else {
		protocol.deleteSegment( currentSegmentNumber - 1 ); // - 1 since parameter is an index
	}
	
	segmentListWidget->clear(); // Clear list view

std::cout<<"Segment deleted from protocol, rebuilding table"<<std::endl;
	
	// Rebuild list view
	QListWidgetItem *element;
	for( int i = 0; i < protocol.numSegments(); i++ ) {
		segmentString = "Segment ";
		if( i < 10 ) // Prefix with zero if a single digit
			segmentString += "0";
		segmentString += QString::number( i + 1 ); // Add number to segment string
		element = new QListWidgetItem( segmentString, segmentListWidget ); // Add segment to list view
		segmentListWidget->addItem(element);
//		element = new QListViewItem( segmentListWidget, segmentString ); // Add segment to list view
	}

std::cout<<"rebuilt list"<<std::endl;
std::cout<<"protocol now has "<<protocol.numSegments()<<" segments"<<std::endl;
	
	// Set to last segment and update table
	if( protocol.numSegments() > 0 ) {
std::cout<<"Elements in segmentListWidget: "<<segmentListWidget->count()<<std::endl;
		segmentListWidget->setCurrentItem( segmentListWidget->item( segmentListWidget->count() - 1 )); // Apparently, qlistwidgets index by 0, which I know now no thanks to Qt's documentation. 
std::cout<<"Current element set to last"<<std::endl;
		updateSegment( segmentListWidget->item(segmentListWidget->count() - 1 ));
std::cout<<"updateSegment returned"<<std::endl;
		updateTable();
std::cout<<"updateTable returned"<<std::endl;
	}
	else { // No segments are left
		currentSegmentNumber = 0;
		protocolTable->setColumnCount( 0 ); // Clear table
		// Prevent resetting of spinbox from triggering slot function by disconnecting
		QObject::disconnect( segmentSweepSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateSegmentSweeps(int)) );
		segmentSweepSpinBox->setValue( 0 ); // Set sweep number spin box to zero
		QObject::connect( segmentSweepSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateSegmentSweeps(int)) );
	}
std::cout<<"Table updated. Add Segment complete."<<std::endl;
}

void ClampProtocolEditor::addStep( void ) { // Adds step to a protocol segment: updates protocol container
	if( currentSegmentNumber == 0 ) {// If no segment exists, return and output error box
		QMessageBox::warning( this,
		"Error",
		"No segment has been created or selected." );
		return ;
	}

std::cout<<"Stargin addStep"<<std::endl;
	
	protocol.addStep( currentSegmentNumber - 1, protocolTable->columnCount() ); // Add step to segment
	
std::cout<<"Step added to protocol"<<std::endl;

	updateTable(); // Rebuild table

std::cout<<"updateTable returned"<<std::endl;
	
	// Set scroll bar all the way to the right when step is added
	QScrollBar *hbar = protocolTable->horizontalScrollBar();
	hbar->setMaximum(hbar->maximum()+100); // Offset of 100 is due to race condition when scroll bar is actually updated
	hbar->setValue(hbar->maximum());
}

void ClampProtocolEditor::insertStep( void ) { // Insert step to a protocol segment: updates protocol container
	if( currentSegmentNumber == 0 ) {// If no segment exists, return and output error box
		QMessageBox::warning( this,
		"Error",
		"No segment has been created or selected." );
		return ;
	}
	
	if( protocolTable->currentColumn() >= 0 ) // If other steps exist
		protocol.addStep( currentSegmentNumber - 1, protocolTable->currentColumn() + 1 ); // Add step to segment
	else // currentColumn() returns -1 if no columns exist
	protocol.addStep( currentSegmentNumber - 1, 0 ); // Add step to segment
	
	updateTable(); // Rebuild table
}

void ClampProtocolEditor::deleteStep( void ) { // Delete step from a protocol segment: updates table, listview, and protocol container
	if( currentSegmentNumber == 0 ) {// If no segment exists, return and output error box
		QMessageBox::warning( this,
		"Error",
		"No segment has been created or selected.");
		return ;
	}
	
	int stepNum = protocolTable->currentColumn(); // Step number that is currently selected
	
	if( stepNum == -1 ) {// If no step exists, return and output error box
		QMessageBox::warning( this,
		"Error",
		"No step has been created or selected." );
		return ;
	}
	
	// Message box asking for confirmation whether step should be deleted
	QString stepString;
	stepString.setNum( stepNum + 1 );
	QString segmentString;
	segmentString.setNum( currentSegmentNumber );
	QString text = "Do you wish to delete Step " + stepString + " of Segment " + segmentString + "?"; // Text pointing out specific segment and step
	bool answer =  QMessageBox::question(this,
	"Delete Step Confirmation",
	text,
	"Yes",
	"No");
	
	if (answer ) // No
		return ;
	else { // Yes
		protocol.deleteStep( currentSegmentNumber - 1, stepNum ); // Erase step from segment
		updateTable(); // Update table
	}
}

void ClampProtocolEditor::createStep( int stepNum ) { // Creates and initializes protocol step
std::cout<<"createStep called with stepNum="<<stepNum<<"and currentSegmentNumber="<<currentSegmentNumber<<std::endl;

	protocolTable->insertColumn( stepNum ); // Insert new column
std::cout<<"added column to protocolTable"<<std::endl;
	QString headerLabel = "Step " + QString( "%1" ).arg( stepNum + 1); // Make header label
	QTableWidgetItem *horizontalHeader = new QTableWidgetItem; 
	horizontalHeader->setText(headerLabel);
	protocolTable->setHorizontalHeaderItem(stepNum, horizontalHeader);
std::cout<<"added horizontalHeader to new column"<<std::endl;
	
	QComboBox *comboItem = new QComboBox(protocolTable);
	Step step = protocol.getStep( currentSegmentNumber - 1, stepNum );
	comboItem->addItems(ampModeList);
	comboItem->setCurrentIndex( step->retrieve(0) );
std::cout<<"set ampMode to "<<step->retrieve(0)<<std::endl;
	protocolTable->setCellWidget( 0, stepNum, comboItem ); // Add amp mode combo box

	QComboBox *comboItem2 = new QComboBox(protocolTable);
	comboItem2->addItems(stepTypeList);
	comboItem2->setCurrentIndex( step->retrieve(1) ); // Set box to retrieved attribute
std::cout<<"set stepType to "<<step->retrieve(1)<<std::endl;
	protocolTable->setCellWidget( 1, stepNum, comboItem2 ); // Add step type combo box
	
	QTableWidgetItem *item;
//	QLineEdit *item;
	QString text;
	// Due to alignment issues, all cells are manually set with CenterAlignTableItem
	// Sets each attribute to its correct valueable
	for( int i = 2; i <= 9; i++ ) {
//		item = new CenterAlignTableItem( protocolTable );
		item = new QTableWidgetItem;
		item->setTextAlignment(Qt::AlignCenter);
//		item = new QLineEdit(protocolTable);
		text.setNum( step->retrieve(i) ); // Retrieve attribute value
		item->setText( text );
		item->setFlags(item->flags() ^ Qt::ItemIsEditable);
		protocolTable->setItem( i, stepNum, item );
//		protocolTable->setCellWidget( i, stepNum, item );
	}
	updateStepAttribute( 1, stepNum ); // Update column based on step type
std::cout<<"createStep returned"<<std::endl<<std::endl;
}

void ClampProtocolEditor::updateSegment( QListWidgetItem *segment ) { // Updates protocol description table when segment is clicked in listview
std::cout<<"updateSegment called"<<std::endl;
	// Update currentSegment to indicate which segment is selected
//	QString label = segment->text( 0 ); // Grab label from selected item in listview
	QString label = segment->text();
std::cout<<label.toStdString()<<std::endl;
	label = label.right( 2 ); // Truncate label to get segment number
	currentSegmentNumber = label.toInt(); // Convert QString to int, now refers to current segment number
	segmentSweepSpinBox->setValue( protocol.numSweeps(currentSegmentNumber - 1) ); // Set sweep number spin box to value stored for particular segment
	updateTableLabel(); // Update label of protocol table
std::cout<<"updateTableLabel returned"<<std::endl<<std::endl;
}

void ClampProtocolEditor::updateSegmentSweeps( int sweepNum ) { // Update container that holds number of segment sweeps when spinbox value is changed
	protocol.setSweeps( currentSegmentNumber - 1, sweepNum ); // Set segment sweep value to spin box value
}

void ClampProtocolEditor::updateTableLabel( void ) { // Updates the label above protocol table to show current selected segment and step
	QString text = "Segment ";
	text.append( QString::number(currentSegmentNumber) );
	int col = protocolTable->currentColumn() + 1;
	if( col != 0 ) {
		text.append(": Step ");
		text.append( QString::number( col ) );
	}
	segmentStepLabel->setText(text);
}

void ClampProtocolEditor::updateTable( void ) { // Updates protocol description table: clears and reloads table from scratch
std::cout<<"updateTable called"<<std::endl;
	protocolTable->setColumnCount( 0 ); // Clear table by setting columns to 0 *Note: deletes QTableItem objects*

std::cout<<"updateTable: table elements deleted"<<std::endl;
	
	// Load steps from current clicked segment into protocol
	int i = 0;
std::cout<<"start loop to load table elements from existing protocol"<<std::endl;
	for( i = 0; i < protocol.numSteps( currentSegmentNumber - 1 ); i++ ) {
std::cout<<i+1<<" of "<<currentSegmentNumber<<" segments"<<std::endl;
		createStep( i ); // Update step in protocol table
	}
std::cout<<"updateTable returned"<<std::endl;
}

void ClampProtocolEditor::updateStepAttribute( int row, int col ) { // Updates protocol container when a table cell is changed
	Step step = protocol.getStep( currentSegmentNumber - 1, col );
//	QComboTableItem *comboItem;
	QComboBox *comboItem;
	QString text;
	bool check;
	
	// Check which row and update corresponding attribute in step container
	switch(row) {
		case 0:
		// Retrieve current item of combo box and set ampMode
			comboItem = qobject_cast<QComboBox*>( protocolTable->cellWidget(row, col) );
			step->ampMode = (ProtocolStep::ampMode_t)comboItem->currentIndex(); 
std::cout<<"Row: "<<row<<"\tampMode: "<<step->ampMode<<std::endl;
			break;

		case 1:
		// Retrieve current item of combo box and set stepType
			comboItem = qobject_cast<QComboBox*>( protocolTable->cellWidget(row, col) );
			step->stepType = (ProtocolStep::stepType_t)comboItem->currentIndex(); 
			updateStepType( col, step->stepType );
std::cout<<"Row: "<<row<<"\tstepType: "<<step->stepType<<std::endl;
			break;
	
		case 2:
			text = protocolTable->item( row, col )->text();
std::cout<<"Row: "<<row<<"\tText: "<<text.toStdString()<<std::endl;
			step->stepDuration = text.toDouble( &check );
			break;
	
		case 3:
//			text = protocolTable->text( row, col );
			text = protocolTable->item( row, col )->text();
std::cout<<"Row: "<<row<<"\tText: "<<text.toStdString()<<std::endl;
			step->deltaStepDuration = text.toDouble( &check );
			break;
	
		case 4:
			text = protocolTable->item( row, col )->text();
std::cout<<"Row: "<<row<<"\tText: "<<text.toStdString()<<std::endl;
			step->holdingLevel1 = text.toDouble( &check );
			break;
	
		case 5:
			text = protocolTable->item( row, col )->text();
std::cout<<"Row: "<<row<<"\tText: "<<text.toStdString()<<std::endl;
			step->deltaHoldingLevel1 = text.toDouble( &check );
			break;
	
		case 6:
			text = protocolTable->item( row, col )->text();
std::cout<<"Row: "<<row<<"\tText: "<<text.toStdString()<<std::endl;
			step->holdingLevel2 = text.toDouble( &check );
			break;
	
		case 7:
			text = protocolTable->item( row, col )->text();
std::cout<<"Row: "<<row<<"\tText: "<<text.toStdString()<<std::endl;
			step->deltaHoldingLevel2 = text.toDouble( &check );
			break;
	
		case 8:
			text = protocolTable->item( row, col )->text();
std::cout<<"Row: "<<row<<"\tText: "<<text.toStdString()<<std::endl;
			step->pulseWidth = text.toDouble( &check );
			break;

		case 9:
			text = protocolTable->item( row, col )->text();
std::cout<<"Row: "<<row<<"\tText: "<<text.toStdString()<<std::endl;
			step->pulseRate = text.toInt( &check );
			break;

		default:
			cout << "Error - ProtocolEditor::updateStepAttribute() - default case" << endl;
			break;
	}
	
	// Check to make sure entries are valid
	if( !check && row > 1 && text != "---" ) {
		if( row == 9 ) QMessageBox::warning( this,"Error", "Pulse rate must be a whole number integer." );
		else QMessageBox::warning( this, "Error", "Step attribute is not a valid number." );
		
		protocolTable->item(row, col)->setText( "0" );
//		protocolTable->setText( row, col, "0" );
	}
}

void ClampProtocolEditor::updateStepType( int stepNum, ProtocolStep::stepType_t stepType ) {
	// Disable unneeded attributes depending on step type
	// Enable needed attributes and set text to stored value
	Step step = protocol.getStep( currentSegmentNumber - 1, stepNum );
	QTableWidgetItem *item;
	
	switch( stepType ){
		case ProtocolStep::STEP:
			for( int i = 6; i <= 9; i++ ) {
				item = protocolTable->item( i, stepNum );
				item->setText( "---" );
				item->setFlags(item->flags() ^ Qt::ItemIsEditable);
//				item->setEnabled( false );
				updateStepAttribute( i, stepNum );
			}
			for( int i = 2; i <= 5; i++ ) {
				item = protocolTable->item( i, stepNum );
				item->setText( QString::number( step->retrieve(i) ) ); // Retrieve attribute and set text
//				item->setEnabled( true );
				item->setFlags(item->flags() ^ Qt::ItemIsEditable);
				updateStepAttribute( i, stepNum );
			}
			break;
	
		case ProtocolStep::RAMP:
			for(int i = 8; i <= 9; i++) {
				item = protocolTable->item( i, stepNum );
				item->setText( "---" );
				item->setFlags(item->flags() ^ Qt::ItemIsEditable);
//				item->setEnabled( false );
				updateStepAttribute( i, stepNum );
			}
			for(int i = 2; i <= 7; i++) {
				item = protocolTable->item( i, stepNum );
				item->setText( QString::number( step->retrieve(i) ) ); // Retrieve attribute and set text
				item->setFlags(item->flags() ^ Qt::ItemIsEditable);
//				item->setEnabled( true );
				updateStepAttribute( i, stepNum );
			}
			break;

		case ProtocolStep::TRAIN:
			for(int i = 2; i <= 7; i++) {
				item = protocolTable->item( i, stepNum );
				item->setText( "---" );
				item->setFlags(item->flags() ^ Qt::ItemIsEditable);
//				item->setEnabled( false );
				updateStepAttribute( i, stepNum );
			}
			for(int i = 8; i <= 9; i++) {
				item = protocolTable->item( i, stepNum );
				item->setText( QString::number( step->retrieve(i) ) ); // Retrieve attribute and set text
				item->setFlags(item->flags() ^ Qt::ItemIsEditable);
//				item->setEnabled( true );
				updateStepAttribute( i, stepNum );
			}
			break;
			/*case ProtocolStep::CUSTOM:
			for(int i = 2; i <= 9; i++) {
				item = protocolTable->item( i,stepNum );
				item->setText("---");
				item->setFlags(item->flags() ^ Qt::ItemIsEditable);
				item->setEnabled(false);
				updateStepAttribute( i, stepNum );
				
			}
			break;*/
	}
}

int ClampProtocolEditor::loadFileToProtocol( QString fileName ) { // Loads XML file of protocol data: updates table, listview, and protocol container
	// If protocol is present, warn user that protocol will be lost upon loading
	if( protocol.numSegments() != 0 &&
		QMessageBox::warning(
			this,
			"Load Protocol", "All unsaved changes to current protocol will be lost.\nDo you wish to continue?",
			QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes )
	return 0; // Return if answer is no
	
	QDomDocument doc( "protocol" );
	QFile file( fileName );
	
	if( !file.open( QIODevice::ReadOnly ) ) { // Make sure file can be opened, if not, warn user
		QMessageBox::warning(this,
		"Error",
		"Unable to open protocol file" );
		return 0;
	}
	if( !doc.setContent( &file ) ) { // Make sure file contents are loaded into document
		QMessageBox::warning(this,
		"Error",
		"Unable to set file contents to document" );
		file.close();
		return 0;
	}
	file.close();
	
	protocol.fromDoc( doc ); // Translate document into protocol
	
	if( protocol.numSegments() < 0 ) {
		QMessageBox::warning(this,
		"Error",
		"Protocol did not contain any segments" );
		return 0;
	}
	
	// Build segment listview
	for( int i = 0; i < protocol.numSegments(); i++ ) {
		QString segmentName = "Segment ";
		if( protocol.numSegments() < 10) // To help with sorting, a zero prefix is used for single digits
			segmentName += "0";
		segmentName += QString::number( i + 1 );
		
		QListWidgetItem *element = new QListWidgetItem(segmentName, segmentListWidget); // Add segment reference to listView
		segmentListWidget->addItem(element);
//		QListViewItem *element = new QListViewItem(segmentListWidget, segmentName); // Add segment reference to listView
	}
	
//	segmentListWidget->setSelected( segmentListWidget->firstChild(), true );
	segmentListWidget->setCurrentItem(segmentListWidget->item(0));
	updateSegment(segmentListWidget->item(0));
	
	return 1;
}


QString ClampProtocolEditor::loadProtocol(void) {
	// Save dialog to retrieve desired filename and location
	QString fileName = QFileDialog::getOpenFileName(
		this,
		"Open a protocol",
		"~/",
		"Clamp Protocol Files (*.csp);;All Files(*.*)");
	
	if( fileName == NULL ) return "";// Null if user cancels dialog
	clearProtocol();
	int retval = loadFileToProtocol(fileName);
	
	if( !retval ) return "";// If error occurs
	
	return fileName;
}

void ClampProtocolEditor::loadProtocol(QString fileName) {
	loadFileToProtocol(fileName);
}

void ClampProtocolEditor::saveProtocol(void) { // Takes data within protocol container and converts to XML and saves to file
	if( protocolEmpty() ) // Exit if protocol is empty
		return ;
	
	protocol.toDoc(); // Update protocol QDomDocument
	
	// Save dialog to retrieve desired filename and location
	QString fileName = QFileDialog::getSaveFileName(
		this,
		"Save the protocol",
		"~/",
		"Clamp Protocol Files (*.csp);;All Files (*.*)");
	
	// If filename does not include .csp extension, add extension
	if( !( fileName.endsWith(".csp") ) ) fileName.append( ".csp" );

	// If filename exists, warn user
	if ( QFileInfo( fileName ).exists() &&
		QMessageBox::warning(this,
			"File Exists", "Do you wish to overwrite " + fileName + "?",
			QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
	return ; // Return if answer is no
	
	// Save protocol to file
	QFile file( fileName ); // Open file
	if( !file.open(QIODevice::WriteOnly) ) { // Open file, return error if unable to do so
		QMessageBox::warning(this, "Error",
		"Unable to save file: Please check folder permissions." );
		return ;
	}
	QTextStream ts( &file ); // Open text stream
	ts << protocol.protocolDoc.toString(); // Write to file
	file.close(); // Close file
}

void ClampProtocolEditor::clearProtocol( void ) { // Clear protocol
	protocol.clear();
	currentSegmentNumber = 0;
	protocolTable->setColumnCount( 0 ); // Clear table
	segmentListWidget->clear();
	
	// Prevent resetting of spinbox from triggering slot function by disconnecting
	QObject::disconnect( segmentSweepSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateSegmentSweeps(int)) );
	segmentSweepSpinBox->setValue( 1 ); // Set sweep number spin box to zero
	QObject::connect( segmentSweepSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateSegmentSweeps(int)) );
}

void ClampProtocolEditor::exportProtocol( void ) { // Export protocol to a text file format ( time : output )
	if( protocolEmpty() ) // Exit if protocol is empty
		return ;
	
	// Grab period
	bool ok;
	double period = QInputDialog::getDouble(
		this,
		"Export Clamp Protocol", "Enter the period (ms): ", 0.010, 0,
		1000, 3, &ok );

	if( !ok ) return;// User cancels
	
	// Save dialog to retrieve desired filename and location
	QString fileName = QFileDialog::getSaveFileName(
		this,
		"Export Clamp Protocol",
		"~/",
		"Text files (*.txt);;All Files (*.*)");
	
	// If filename does not include .txt extension, add extension
	if( !( fileName.endsWith(".txt") ) ) fileName.append( ".txt" );

	// If filename exists, warn user
	if ( QFileInfo( fileName ).exists() &&
		QMessageBox::warning(
		this,
		"File Exists", "Do you wish to overwrite " + fileName + "?",
		QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
	return ; // Return if answer is no
	
	// Save protocol to file
	QFile file( fileName ); // Open file
	if( !file.open(QIODevice::WriteOnly) ) { // Open file, return error if unable to do so
		QMessageBox::warning(this,
		"Error",
		"Unable to save file: Please check folder permissions." );
		return ;
	}
	
	if( fileName == NULL ) // Null if user cancels dialog
		return ;
	
	// Run protocol with user specified period
	std::vector< std::vector<double> > run;
	run = protocol.run( period );
	std::vector<double> time = run.at( 0 );
	std::vector<double> output = run.at( 1 );
	
	QTextStream ts( &file );
	
	for( std::vector<double>::iterator itx = time.begin(), ity = output.begin();
		itx < time.end(); itx++, ity++ ) { // Iterate through vectors and output to file
		ts << *itx << " " << *ity << "\n";
	}
	
	file.close(); // Close file
}

void ClampProtocolEditor::previewProtocol( void ) { // Graph protocol output in a simple plot window
	if( protocolEmpty() ) // Exit if protocol is empty
		return ;
	
	// Create a dialog with a BasicPlot
	QDialog *dlg = new QDialog( this , Qt::Dialog );
	dlg->setAttribute(Qt::WA_DeleteOnClose );
	dlg->setWindowTitle( "Protocol Preview" );
	QVBoxLayout *layout = new QVBoxLayout( dlg );
	QwtPlot *plot = new QwtPlot( dlg );
	layout->addWidget( plot );
	dlg->resize( 500, 500 );
	dlg->show();
	
	// Plot Settings
	plot->setCanvasBackground(QColor(70, 128, 186));
	QwtText xAxisTitle, yAxisTitle;
	xAxisTitle.setText( "Time (ms)" );
	yAxisTitle.setText( "Voltage (mV)" );
	plot->setAxisTitle( QwtPlot::xBottom, xAxisTitle );
	plot->setAxisTitle( QwtPlot::yLeft, yAxisTitle );
	plot->show();
	
	// Run protocol and plot
	std::vector< std::vector<double> > run;
	run = protocol.run( 0.01 );
	
	std::vector<double> timeVector, outputVector;
	timeVector = run.at( 0 );
	outputVector = run.at( 1 );
	
	QwtPlotCurve *curve = new QwtPlotCurve( "" ); // Will be deleted when preview window is closed
	curve->setSamples( &timeVector[0], &outputVector[0], timeVector.size() ); // Makes a hard copy of both time and output
	curve->attach( plot );
	plot->replot();
}

bool ClampProtocolEditor::protocolEmpty( void ) { // Make sure protocol has at least one segment with one step
	if( protocol.numSegments() == 0) { // Check if first segment exists
		QMessageBox::warning(this,
		"Error",
		"A protocol must contain at least one segment that contains at least one step" );
		return true;
	}
	else if( protocol.numSteps( 0 ) == 0 ) { // Check if first segment has a step
		QMessageBox::warning(this,
		"Error",
		"A protocol must contain at least one segment that contains at least one step" );
		return true;
	}
	
	return false;
}

void ClampProtocolEditor::createGUI(void) {
	QWidget::setAttribute(Qt::WA_DeleteOnClose);

	subWindow = new QMdiSubWindow;
	subWindow->setWindowIcon(QIcon("/usr/local/lib/rtxi/RTXI-widget-icon.png"));
	subWindow->setWindowFlags(Qt::CustomizeWindowHint);
	subWindow->setWindowFlags(Qt::WindowCloseButtonHint);
//	subWindow->setOption(QMdiSubWindow::RubberBandResize, true);
//	subWindow->setOption(QMdiSubWindow::RubberBandMove, true);
	MainWindow::getInstance()->createMdi(subWindow);

	windowLayout = new QVBoxLayout(this);
	subWindow->setLayout(windowLayout);

	layout1 = new QHBoxLayout;
	QHBoxLayout *layout1_left = new QHBoxLayout;
	layout1_left->setAlignment(Qt::AlignLeft);
	QHBoxLayout *layout1_right = new QHBoxLayout;
	layout1_right->setAlignment(Qt::AlignRight);
	
	saveProtocolButton = new QPushButton("Save");
	saveProtocolButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	loadProtocolButton = new QPushButton("Load");
	loadProtocolButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	exportProtocolButton = new QPushButton("Export");
	exportProtocolButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	previewProtocolButton = new QPushButton("Preview");
	previewProtocolButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	clearProtocolButton = new QPushButton("Clear");
	clearProtocolButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	
	layout1_left->addWidget(saveProtocolButton);
	layout1_left->addWidget(loadProtocolButton);
	layout1_right->addWidget(exportProtocolButton);
	layout1_right->addWidget(previewProtocolButton);
	layout1_right->addWidget(clearProtocolButton);
	layout1->addLayout(layout1_left);
	layout1->addLayout(layout1_right);
	windowLayout->addLayout(layout1);

	layout2 = new QGridLayout;
	layout3 = new QVBoxLayout;

	protocolDescriptionBox = new QGroupBox("Protocol Steps");
	protocolDescriptionBoxLayout = new QVBoxLayout;
	protocolDescriptionBox->setLayout(protocolDescriptionBoxLayout);

	segmentStepLabel = new QLabel("Step");
	segmentStepLabel->setAlignment(Qt::AlignCenter);
	protocolDescriptionBoxLayout->addWidget(segmentStepLabel);

	protocolTable = new QTableWidget;
	protocolDescriptionBoxLayout->addWidget(protocolTable);
   protocolTable->setRowCount(10);
   protocolTable->setColumnCount(0);
   QStringList rowLabels = ( QStringList() << "Amplifier Mode"
                                          << "Step Type"
                                          << "Step Duration"
                                          << QString::fromUtf8("\xce\x94\x20\x53\x74\x65\x70\x20\x44\x75\x72\x61\x74\x69\x6f\x6e\x20\x28\x6d\x73\x29")
                                          << "Hold Level 1"
                                          << QString::fromUtf8("\xce\x94\x20\x48\x6f\x6c\x64\x69\x6e\x67\x20\x4c\x65\x76\x65\x6c\x20\x31\x20\x28\x6d\x56\x2f\x70\x41\x29")
                                          << "Hold Level 2"
                                          << QString::fromUtf8("\xce\x94\x20\x48\x6f\x6c\x64\x69\x6e\x67\x20\x4c\x65\x76\x65\x6c\x20\x31\x20\x28\x6d\x56\x2f\x70\x41\x29")
                                          << "Pulse Width (ms)"
                                          << "Puse Train Rate" );
   protocolTable->setVerticalHeaderLabels(rowLabels);
	protocolTable->setSelectionBehavior(QAbstractItemView::SelectItems);
	protocolTable->setSelectionMode(QAbstractItemView::SingleSelection);
	protocolTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
//	protocolTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	protocolTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
	protocolTable->setMinimumHeight(340);//FixedHeight(400);
	protocolDescriptionBoxLayout->addWidget(protocolTable);

	layout3->addWidget(protocolDescriptionBox);

	layout4 = new QHBoxLayout;
	layout4->setAlignment(Qt::AlignLeft);
	addStepButton = new QPushButton("Add Step"); //Add Step
	addStepButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	insertStepButton = new QPushButton("Insert Step"); //Insert Step
	insertStepButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	deleteStepButton = new QPushButton("Delete Step"); //Delete Step
	deleteStepButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	layout4->addWidget(addStepButton);
	layout4->addWidget(insertStepButton);
	layout4->addWidget(deleteStepButton);

	layout3->addLayout(layout4);
	layout2->addLayout(layout3, 1, 1, 1, 2);
	layout2->setColumnMinimumWidth(1, 505);
	layout2->setColumnStretch (1, 1);

	layout5 = new QVBoxLayout;
	segmentSummaryGroup = new QGroupBox("Segment Summary");
//	segmentSummaryGroup->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	segmentSummaryGroupLayout = new QVBoxLayout;
	segmentSummaryGroup->setLayout(segmentSummaryGroupLayout);

//	segmentSweepGroup = new QGroupBox;
	segmentSweepGroupLayout = new QHBoxLayout;
//	segmentSweepGroup->setLayout(segmentSweepGroupLayout);
//	segmentSweepGroupLayout->setAlignment(Qt::AlignTop);

	segmentSweepLabel = new QLabel("Sweeps");
	segmentSweepSpinBox = new QSpinBox;
	segmentSweepGroupLayout->addWidget(segmentSweepLabel);
	segmentSweepGroupLayout->addWidget(segmentSweepSpinBox);

	segmentSummaryGroupLayout->addLayout(segmentSweepGroupLayout);

	segmentListWidget = new QListWidget;
//	segmentListWidget->addColumns("Segment #");
//	segmentListWidget->addItem("Segment #");
//	segmentListWidget->header()->setResizeEnabled(FALSE, segmentListWidget->header()->count()-1);
//	segmentListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	segmentListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
//	segmentListWidget->setResizeMode(QListView::LastColumn);
//	segmentListWidget->setTreeStepSize(20);
	segmentSummaryGroupLayout->addWidget(segmentListWidget);
	layout5->addWidget(segmentSummaryGroup);

	layout6 = new QHBoxLayout;
	layout6->setAlignment(Qt::AlignRight);
	addSegmentButton = new QPushButton("Add Segment"); //Add Segment
	addSegmentButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	deleteSegmentButton = new QPushButton("Delete Segment"); // Delete Segment
	deleteSegmentButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	layout6->addWidget(addSegmentButton);
	layout6->addWidget(deleteSegmentButton);
	layout5->addLayout(layout6);
	layout2->addLayout(layout5, 1, 3, 1, 1);
	layout2->setColumnStretch(3, 0);
	windowLayout->addLayout(layout2);
//	clearWState(WState_Polished);

	// Signal and slot connections for protocol editor UI
	QObject::connect( protocolTable, SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(updateTableLabel(void)) );
	QObject::connect( addSegmentButton, SIGNAL(clicked(void)), this, SLOT(addSegment(void)) );
	QObject::connect( segmentListWidget, SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(updateSegment(QListWidgetItem*)) );
	QObject::connect( segmentListWidget, SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(updateTable(void)) );
	QObject::connect( segmentListWidget, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(updateSegment(QListWidgetItem*)) );
	QObject::connect( segmentListWidget, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(updateTable(void)) );
	QObject::connect( segmentSweepSpinBox,  SIGNAL(valueChanged(int)), this, SLOT(updateSegmentSweeps(int)) );
	QObject::connect( addStepButton, SIGNAL(clicked(void)), this, SLOT(addStep(void)) );
	QObject::connect( insertStepButton, SIGNAL(clicked(void)), this, SLOT(insertStep(void)) );
	QObject::connect( protocolTable, SIGNAL(cellChanged(int,int)), this, SLOT(updateStepAttribute(int,int)) );
	QObject::connect( deleteStepButton, SIGNAL(clicked(void)), this, SLOT(deleteStep(void)) );
	QObject::connect( deleteSegmentButton, SIGNAL(clicked(void)), this, SLOT(deleteSegment(void)) );
	QObject::connect( saveProtocolButton, SIGNAL(clicked(void)), this, SLOT(saveProtocol(void)) );
	QObject::connect( loadProtocolButton, SIGNAL(clicked(bool)), this, SLOT(loadProtocol(void)) );
	QObject::connect( clearProtocolButton, SIGNAL(clicked(void)), this, SLOT(clearProtocol(void)) );
	QObject::connect( exportProtocolButton, SIGNAL(clicked(void)), this, SLOT(exportProtocol(void)) );
	QObject::connect( previewProtocolButton, SIGNAL(clicked(void)), this, SLOT(previewProtocol(void)));
   
	subWindow->setWidget(this);
   show();
	subWindow->adjustSize();
}

void ClampProtocolEditor::protocolTable_currentChanged(int, int) {
	qWarning( "ProtocolEditorUI::protocolTable_currentChanged(int,int): Not implemented yet" );
}

void ClampProtocolEditor::protocolTable_verticalSliderReleased(void) {
	qWarning( "ProtocolEditorUI::protocolTable_verticalSliderReleased(void): Not implemented yet" );
}
