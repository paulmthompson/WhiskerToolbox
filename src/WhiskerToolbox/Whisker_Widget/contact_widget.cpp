
#include "contact_widget.hpp"

#include "ui_contact_widget.h"

#include <QFileDialog>

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

Contact_Widget::Contact_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    _data_manager{data_manager},
    _contact_start{0},
    _contact_epoch(false),
    _contactEvents{std::vector<ContactEvent>()},
    ui(new Ui::contact_widget)
{
    ui->setupUi(this);

    _scene = new QGraphicsScene();

    ui->graphicsView->setScene(_scene);
    ui->graphicsView->show();
};

Contact_Widget::~Contact_Widget() {
    delete ui;
}

void Contact_Widget::openWidget() {

    connect(ui->contact_button, SIGNAL(clicked()), this, SLOT(_contactButton()));
    connect(ui->save_contact_button, SIGNAL(clicked()), this, SLOT(_saveContact()));
    connect(ui->load_contact_button, SIGNAL(clicked()), this, SLOT(_loadContact()));

    if (_contact.empty()) {
        _contact = std::vector<Contact>(_data_manager->getTime()->getTotalFrameCount());
    }

    this->show();

}

void Contact_Widget::closeEvent(QCloseEvent *event) {

    disconnect(ui->contact_button, SIGNAL(clicked()), this, SLOT(_contactButton()));
    disconnect(ui->save_contact_button, SIGNAL(clicked()), this, SLOT(_saveContact()));
    disconnect(ui->load_contact_button, SIGNAL(clicked()), this, SLOT(_loadContact()));
}

void Contact_Widget::_contactButton() {

    auto frame_num = _data_manager->getTime()->getLastLoadedFrame();

    // If we are in a contact epoch, we need to mark the termination frame and add those to block
    if (_contact_epoch) {

        _contact_epoch = false;

        ui->contact_button->setText("Mark Contact");

        for (int i = _contact_start; i < frame_num; i++) {
            _contact[i] = Contact::Contact;
        }

    } else {
        // If we are not already in contact epoch, start one
        _contact_start = frame_num;

        _contact_epoch = true;

        ui->contact_button->setText("Mark Contact End");
    }

    _calculateContactPeriods();
}

void Contact_Widget::_saveContact() {


    std::fstream fout;

    fout.open("contact.csv", std::fstream::out);

    for (auto &frame_contact: _contact) {
        if (frame_contact == Contact::Contact) {
            fout << "Contact" << "\n";
        } else {
            fout << "Nocontact" << "\n";
        }
    }

    fout.close();
}

void Contact_Widget::_loadContact() {

    auto contact_filename = QFileDialog::getOpenFileName(
        this,
        "Load Video File",
        QDir::currentPath(),
        "All files (*.*) ;; MP4 (*.mp4)");


    std::ifstream fin(contact_filename.toStdString());

    std::string line;

    int row_num = 0;

    while (std::getline(fin, line)) {
        if (line == "Contact") {
            _contact[row_num] = Contact::Contact;
        }
        row_num++;
    }

    fin.close();

    _calculateContactPeriods();
}

void Contact_Widget::_buildContactTable()
{
    ui->contact_table->setRowCount(0);
    for (int i=0; i < _contactEvents.size(); i++)
    {
        ui->contact_table->insertRow(ui->contact_table->rowCount());
        ui->contact_table->setItem(i,0,new QTableWidgetItem(QString::number(_contactEvents[i].start)));
        ui->contact_table->setItem(i,1,new QTableWidgetItem(QString::number(_contactEvents[i].end)));

    }
}

void Contact_Widget::_calculateContactPeriods()
{
    bool in_contact = false;
    int contact_start = 0;

    _contactEvents = std::vector<ContactEvent>();

    for (int i = 0; i < _contact.size(); i++)
    {
        if (in_contact)
        {
            if (_contact[i] == Contact::NoContact) {
                _contactEvents.push_back(ContactEvent{contact_start,i});
                in_contact = false;
            }
        } else {
            if (_contact[i] == Contact::Contact)
            {
                in_contact = true;
                contact_start = i;
            }
        }
    }

    std::cout << "There are " << _contactEvents.size() << " contact events" << std::endl;
    _buildContactTable();
}
