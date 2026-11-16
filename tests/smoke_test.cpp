#include <QString>
#include <gtest/gtest.h>

TEST(QtCore, QStringRoundTripsThroughStdString) {
    const QString text = QStringLiteral("waterfall");
    EXPECT_EQ(text.toStdString(), "waterfall");
}
