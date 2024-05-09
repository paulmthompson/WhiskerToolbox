
#include "contact_widget.hpp"

#include "ui_contact_widget.h"

#include <QFileDialog>

#include <sstream>
#include <fstream>
#include <string>

Contact_Widget::Contact_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    _data_manager{data_manager},
    _contact_start{0},
    _contact_epoch(false),
    ui(new Ui::contact_widget) {
    ui->setupUi(this);
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
}
