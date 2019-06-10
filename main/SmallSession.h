/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Vect
    An experimental audio player for plural recordings of a work
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef VECT_SMALL_SESSION_H
#define VECT_SMALL_SESSION_H

#include <vector>
#include <QString>

/**
 * Just a container for the origin URIs of the files in a session,
 * with load/save from/to XML.
 */
struct SmallSession
{
    QString mainFile;
    std::vector<QString> additionalFiles;

    /**
     * Save the given session to the given filename.
     * Throw std::runtime_error if the save fails.
     */
    static void save(const SmallSession &session, QString toSmallSessionFile);

    /**
     * Load a session from the given session file.
     * Throw std::runtime_error if the load fails.
     */
    static SmallSession load(QString fromSmallSessionFile);
};

#endif

