#ifndef DOCUMENTNUMBERGENERATOR_H
#define DOCUMENTNUMBERGENERATOR_H

#include <QSqlDatabase>
#include <QString>

// Генерация номера документа. Обёртка над DatabaseManager::generateDocNumber.
class DocumentNumberGenerator {
public:
    // Возвращает следующий номер для docType ("receipt", "rental", ...)
    // или пустую строку при ошибке.
    static QString generate(const QString& docType, QSqlDatabase& db);
};

#endif // DOCUMENTNUMBERGENERATOR_H
