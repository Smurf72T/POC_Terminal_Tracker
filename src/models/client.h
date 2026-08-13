#ifndef CLIENT_H
#define CLIENT_H

#include <QString>

// Value-модель клиента (tblclients). Наполняется через ClientRepository.
namespace models {

struct Client {
    int id = 0;
    QString name;
    QString inn;
    QString address;
    QString contactPhone;
    QString contactEmail;
};

} // namespace models

#endif // CLIENT_H