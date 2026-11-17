#pragma once

#include <QDialog>

class QPushButton;
class QShowEvent;
class QTextBrowser;

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);

    QTextBrowser* text() const { return m_text; }
    QPushButton* closeButton() const { return m_close; }

protected:
    void showEvent(QShowEvent* event) override;

private:
    QTextBrowser* m_text = nullptr;
    QPushButton* m_close = nullptr;
    bool m_fullScreenApplied = false;
};
