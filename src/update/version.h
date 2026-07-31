#ifndef VERSION_H
#define VERSION_H

#include <QString>
#include <QStringList>

namespace UpdateUtils {

struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

inline bool parseVersion(const QString &text, Version &out)
{
    QString s = text.trimmed();
    int dash = s.indexOf('-');
    if (dash >= 0)
        s = s.left(dash);

    QStringList parts = s.split('.');
    if (parts.isEmpty() || parts.size() > 3)
        return false;

    Version v;
    bool ok = false;
    v.major = parts[0].trimmed().toInt(&ok);
    if (!ok)
        return false;
    if (parts.size() >= 2) {
        v.minor = parts[1].trimmed().toInt(&ok);
        if (!ok)
            return false;
    }
    if (parts.size() >= 3) {
        v.patch = parts[2].trimmed().toInt(&ok);
        if (!ok)
            return false;
    }

    out = v;
    return true;
}

inline int compareVersions(const QString &a, const QString &b)
{
    Version va, vb;
    if (!parseVersion(a, va) || !parseVersion(b, vb))
        return 0;

    if (va.major != vb.major)
        return va.major < vb.major ? -1 : 1;
    if (va.minor != vb.minor)
        return va.minor < vb.minor ? -1 : 1;
    if (va.patch != vb.patch)
        return va.patch < vb.patch ? -1 : 1;
    return 0;
}

inline bool isVersionNewer(const QString &candidate, const QString &current)
{
    return compareVersions(candidate, current) > 0;
}

} // namespace UpdateUtils

#endif // VERSION_H
