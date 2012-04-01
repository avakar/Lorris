/****************************************************************************
**
**    This file is part of Lorris.
**    Copyright (C) 2012 Vojtěch Boček
**
**    Contact: <vbocek@gmail.com>
**             https://github.com/Tasssadar
**
**    Lorris is free software: you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation, either version 3 of the License, or
**    (at your option) any later version.
**
**    Lorris is distributed in the hope that it will be useful,
**    but WITHOUT ANY WARRANTY; without even the implied warranty of
**    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**    GNU General Public License for more details.
**
**    You should have received a copy of the GNU General Public License
**    along with Lorris.  If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/

#include "scriptstorage.h"
#include "analyzerdatafile.h"

ScriptStorage::ScriptStorage(QObject *parent) :
    QObject(parent)
{
}

ScriptStorage::~ScriptStorage()
{
    clear();
}

void ScriptStorage::clear()
{
    for(DataHash::iterator itr = m_data.begin(); itr != m_data.end(); ++itr)
    {
        delete[] (*itr)->data;
        delete *itr;
    }
    m_data.clear();
}

ScriptData *ScriptStorage::findKey(const QString &key)
{
    DataHash::iterator itr = m_data.find(key);
    if(itr != m_data.end())
        return *itr;
    return NULL;
}

ScriptData *ScriptStorage::getKeyForSet(const QString &key)
{
    ScriptData *data = findKey(key);

    if(data)
    {
        delete[] data->data;
        data->data = NULL;
        data->len = 0;
    }
    else
    {
        data = new ScriptData();
        m_data.insert(key, data);
    }
    return data;
}

void ScriptStorage::saveToFile(AnalyzerDataFile *file)
{
    file->writeBlockIdentifier("scriptWDataStorage");

    quint32 size = m_data.size();
    file->write((char*)(&size), sizeof(size));

    for(DataHash::iterator itr = m_data.begin(); itr != m_data.end(); ++itr)
    {
        ScriptData *data = *itr;

        QByteArray key = itr.key().toUtf8();
        quint32 key_len = key.size();

        file->write((char*)&key_len, sizeof(quint32));
        file->write(key.data(), key_len);

        file->write((char*)(&data->len), sizeof(data->len));
        file->write(data->data, data->len);
    }
}

void ScriptStorage::loadFromFile(AnalyzerDataFile *file)
{
    if(!file->seekToNextBlock("scriptWDataStorage", BLOCK_WIDGET))
        return;

    quint32 size = 0;
    file->read((char*)(&size), sizeof(size));

    for(quint32 i = 0; i < size; ++i)
    {
        quint32 key_len = 0;
        file->read((char*)&key_len, sizeof(key_len));

        QString key = QString::fromUtf8(file->read(key_len), key_len);

        ScriptData *data = new ScriptData();
        file->read((char*)(&data->len), sizeof(data->len));

        data->data = new char[data->len];
        file->read(data->data, data->len);

        m_data.insert(key, data);
    }
}

void ScriptStorage::setBool(const QString& key, bool val)
{
    setBaseType(key, val);
}

void ScriptStorage::setUInt32(const QString& key, quint32 val)
{
    setBaseType(key, val);
}

void ScriptStorage::setInt32(const QString& key, qint32 val)
{
    setBaseType(key, val);
}

void ScriptStorage::setFloat(const QString& key, float val)
{
    setBaseType(key, val);
}

void ScriptStorage::setString(const QString &key, const QString &val)
{
    ScriptData *sc_data = getKeyForSet(key);

    QByteArray val_byte = val.toUtf8();

    sc_data->len = val_byte.size();
    sc_data->data = new char[sc_data->len];

    std::copy(val_byte.data(), val_byte.data()+sc_data->len, sc_data->data);
}

void ScriptStorage::setInt32Array(const QString &key, const QVariantList &val)
{
    setArrayType<qint32>(key, val);
}

void ScriptStorage::setUInt32Array(const QString &key, const QVariantList &val)
{
    setArrayType<quint32>(key, val);
}

void ScriptStorage::setFloatArray(const QString &key, const QVariantList &val)
{
    setArrayType<float>(key, val);
}

bool ScriptStorage::getBool(const QString &key)
{
    return getBaseType<bool>(key);
}

quint32 ScriptStorage::getUInt32(const QString &key)
{
    return getBaseType<quint32>(key);
}

qint32 ScriptStorage::getInt32(const QString &key)
{
    return getBaseType<qint32>(key);
}

float ScriptStorage::getFloat(const QString &key)
{
    return getBaseType<float>(key);
}

QString ScriptStorage::getString(const QString &key)
{
    ScriptData *sc_data = findKey(key);
    if(!sc_data)
        return "";

    return QString::fromUtf8(sc_data->data, sc_data->len);
}

QVariantList ScriptStorage::getInt32Array(const QString &key)
{
    return getArrayType<qint32>(key);
}

QVariantList ScriptStorage::getUInt32Array(const QString &key)
{
    return getArrayType<quint32>(key);
}

QVariantList ScriptStorage::getFloatArray(const QString &key)
{
    return getArrayType<float>(key);
}