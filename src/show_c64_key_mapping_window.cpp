//////////////////////////////////////////////////
//                                              //
// Emu64                                        //
// von Thorsten Kattanek                        //
//                                              //
// #file: show_c64_key_mapping_window.cpp       //
//                                              //
// Dieser Sourcecode ist Copyright geschützt!   //
// Geistiges Eigentum von Th.Kattanek           //
//                                              //
// Letzte Änderung am 16.08.2019                //
// www.emu64.de                                 //
//                                              //
//////////////////////////////////////////////////

#include "show_c64_key_mapping_window.h"
#include "ui_show_c64_key_mapping_window.h"

ShowC64KeyMappingWindow::ShowC64KeyMappingWindow(QWidget *parent, C64Class *c64) :
    QDialog(parent),
    ui(new Ui::ShowC64KeyMappingWindow)
{
    this->c64 = c64;
    ui->setupUi(this);

    QFontDatabase fontDB;
    fontDB.addApplicationFont(":/fonts/lucon.ttf");
    QFont font1("Lucida Console",10);

    ui->text_out->setFont(font1);

    UpdateText();
}

ShowC64KeyMappingWindow::~ShowC64KeyMappingWindow()
{
    delete ui;
}

void ShowC64KeyMappingWindow::UpdateText()
{
    ui->text_out->clear();

    ui->text_out->appendPlainText(tr("C64 Taste     |       SDL Taste"));
    ui->text_out->appendPlainText(tr("--------------+----------------"));

    int c64_key_table_size = c64->GetC64KeyTableSize();
    C64_KEYS *c64_key_table = c64->GetC64KeyTable();
    const char** c64_key_names = c64->GetC64KeyNameTable();

    for(int i=0; i<c64_key_table_size; i++)
    {
        QString line = QString(tr(c64_key_names[i])) + " ";
        int len = line.length();
        for(int j=0; j<(14-len); j++)
        {
            line += ".";
        }

        line += "|";

        //const char* sdl_key_name = SDL_GetKeyName(c64_key_table[i].SDLKeyCode);
        QString sdl_key_name = QString(tr(SDL_GetKeyName(c64_key_table[i].SDLKeyCode)));

        len = sdl_key_name.length();
        for(int j=0; j<(15-len); j++)
        {
            line += ".";
        }

        line += " " + sdl_key_name;

        ui->text_out->appendPlainText(line);
    }

    ui->text_out->moveCursor (QTextCursor::Start) ;
}
