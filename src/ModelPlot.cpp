/*
 * Copyright (c) 2009 Mark Liversedge (liversedge@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "ModelPlot.h"
#include "ModelWindow.h"
#include "MainWindow.h"
#include "Zones.h"
#include "RideFile.h"
#include "Units.h" // for MILES_PER_KM

#include <QWidget>

using namespace Qwt3D; // namespace ref is only visible in this file (is not in any headers)

/*----------------------------------------------------------------------
 * MODEL DATA PROVIDER
 *
 * This function is called by the surface plot to get z values for an
 * xy pair. The data is set by a call from ModelPlot (from ModelWindow)
 * when a user either selects a new ridefile or changes the x/y/z/color
 * comboboxes.
 *
 * setData   - sets the model data from a ridefile using the axis passed
 *             and returns the mesh values as an xy to set the surface
 *             plot. Will clear existing data first.
 *
 * operator () - called by the surface plot to set z for a given x and y
 *
 *----------------------------------------------------------------------*/

// util function to create an x/y key for QHash, effective and quick enough
static QString xystring(double x, double y) { return QString("%1:%2").arg((int)x).arg((int)y); }
#if 0
static void unxystring(QString val, double &x, double &y)
{
    QRegExp it("^([^:]*):([^:]*)$");
    it.exactMatch(val);
    x = it.cap(1).toDouble();
    y = it.cap(2).toDouble();
}
#endif

// returns the color for an xyz point
class ModelDataColor : public Color
{
        // return RGBA color for x,y,z
        Qwt3D::RGBA operator () (double x, double y, double) const
        {
            QColor cHSV, cRGB;
            double val = color.value(xystring(x,y), 0.0);
            RGBA colour;
            if (!val) {
                return RGBA(255,255,255,0); // see thru
            } if (iszones == true) { // zone 0 is set to zone 1 to distinguish from no value
                cHSV = zonecolor.value(val-1, QColor(0,0,0));
            } else if (val) {
                cHSV.setHsv((double)255 * (double)((val-min)/(max-min)), 255, 255);
            }
            cRGB = cHSV.convertTo(QColor::Rgb);
            colour.r = (double) cRGB.red()/(double)255;
            colour.g = (double) cRGB.green()/(double)255;
            colour.b = (double) cRGB.blue()/(double)255;
            colour.a = 1.0;
            return colour;
        }

        Qwt3D::ColorVector &createVector(Qwt3D::ColorVector &vec)
        {
            RGBA colour;
            QColor cHSV, cRGB;
            Qwt3D::ColorVector::iterator it;

            // clear current
            vec.clear();

            // update the color vector to use our color scheme
            if (iszones) {
                for (int i=0; i< 7; i++) {
                    cRGB = zonecolor.value(i, QColor(0,0,0)).convertTo(QColor::Rgb);
                    colour.r = (double) cRGB.red()/(double)255;
                    colour.g = (double) cRGB.green()/(double)255;
                    colour.b = (double) cRGB.blue()/(double)255;
                    colour.a = 1.0;

                    it = vec.end();
                    vec.insert(it, 1, colour);
                }
            } else {
                double val;
                int i;
                // just add 100 colors graded from min to max
                for (i=0, val=min; i<100; i++, val += ((max-min)/100)) {

                    cHSV.setHsv((double)255 * (double)((val-min)/(max-min)), 255, 255);
                    cRGB = cHSV.convertTo(QColor::Rgb);
                    colour.r = (double) cRGB.red()/(double)255;
                    colour.g = (double) cRGB.green()/(double)255;
                    colour.b = (double) cRGB.blue()/(double)255;
                    colour.a = 1.0;
                    it = vec.end();
                    vec.insert(it, 1, colour);
                }
            }
            return vec;
        }

        public:
            QHash<QString, double> color;
            QHash<QString, int> num;      // xy map with count of values for averaging
            double min, max;

            bool iszones; // if the color value is a zone number
            QMap<double, QColor> zonecolor;

};


// Sorry Sean et al. These local statics are a nasty'ism I'll
// look to tidy later. I cannot reference a class for these
// since qwtplot3d manages its data providers internally with clones
// and when it calls an enricher the data provider is not made
// available.
//
// Ideally they would be held within BasicModelPlot::dataModelProvider
// and were until I needed to implement a custom enrichment to
// support interval selection/display
//
// As a result I use these to allow the DataModelProvider to
// share data with the Bar enrichment. I also tried to use a ref
// to the dataModelProvider but it caused a nasty SEGV
//
// Lets just turn a blind eye ;-)
//

#define SHOW_INTERVALS 1
#define SHOW_FRAME       2

static double diag_;
static int   intervals_;                // SHOW_INTERVALS | SHOW_MAX
static double zpane=0;
static QHash<QString, double> iz;         // for selected intervals
static QHash<QString, double> inum;      // for selected intervals

class ModelDataProvider : public Function
{

    public:

        ModelDataProvider (BasicModelPlot &plot, ModelSettings *settings);
        void setData(RideFile *ride, int x, int y, int z, int col); // set the maps and return mesh dimension

        // return z value for x,y - std qwt3plot method
        double operator () (double x, double y)
        {
            // return the z value for x and y
            return mz.value(xystring(x,y), 0.0);
        }
        double intervals (double x, double y) // return value for selected intervals
        {
            return iz.value(xystring(x,y), 0.0)-minz;
        }
        double getMinz() { return minz; }
        double getMaxz() { return maxz; }

        ~ModelDataProvider()
        {
            mz.clear();
            mnum.clear();
            iz.clear();
            inum.clear();
        }

    private:

        QHash<QString, double> mz;        // xy map with max z values;
        QHash<QString, int> mnum;      // xy map with count of values for averaging


        double pointType(const RideFilePoint *, int);
        QString describeType(int, bool);
        double maxz, minz;
};

double
ModelDataProvider::pointType(const RideFilePoint *point, int type)
{
    // return the point value for the given type
    switch(type) {

        case MODEL_POWER : return point->watts;
        case MODEL_CADENCE : return point->cad;
        case MODEL_HEARTRATE : return point->hr;
        case MODEL_SPEED : return point->kph;
        case MODEL_ALT : return point->alt;
        case MODEL_PEDALFORCE : return point->nm;
        case MODEL_TIME : return point->secs;
        case MODEL_DISTANCE : return point->km;
        case MODEL_INTERVAL : return point->interval;
        case MODEL_LAT : return point->lat;
        case MODEL_LONG : return point->lon;

        // these you need to do yourself cause there is some
        // logic needed and I'm just lookup table!
        case MODEL_XYTIME : return 1;
        case MODEL_POWERZONE : return point->watts;
    }
    return 0; // ? unknown channel ?
}

QString
ModelDataProvider::describeType(int type, bool longer)
{
    // return the point value for the given type
    if (longer == true) {
        switch(type) {

            case MODEL_POWER : return ("Power (watts)");
            case MODEL_CADENCE : return ("Cadence (rpm)");
            case MODEL_HEARTRATE : return ("Heartrate (bpm)");
            case MODEL_SPEED : return ("Speed (kph)"); //XXX metric / imperial!
            case MODEL_ALT : return ("Altitude (meters)"); // XXX metric / imperial
            case MODEL_PEDALFORCE : return ("Pedal Force (nm)");
            case MODEL_TIME : return ("Elapsed Time (secs)");
            case MODEL_DISTANCE : return ("Elapsed Distance (km)"); //XXX metric/imperial
            case MODEL_INTERVAL : return ("Interval Number"); // XXX implemented differently
            case MODEL_LAT : return ("Latitude (degree offset)");
            case MODEL_LONG : return ("Longitude (degree offset)");

            // these you need to do yourself cause there is some
            // logic needed and I'm just lookup table!
            case MODEL_XYTIME : return ("Time at X/Y (%)");
            case MODEL_POWERZONE : return ("Power Zone");
        }
        return ("Unknown");; // ? unknown channel ?
    } else {
        switch(type) {

            case MODEL_POWER : return ("Power");
            case MODEL_CADENCE : return ("Cadence");
            case MODEL_HEARTRATE : return ("Heartrate");
            case MODEL_SPEED : return ("Speed"); //XXX metric / imperial!
            case MODEL_ALT : return ("Altitude"); // XXX metric / imperial
            case MODEL_PEDALFORCE : return ("Pedal Force");
            case MODEL_TIME : return ("Time");
            case MODEL_DISTANCE : return ("Distance"); //XXX metric/imperial
            case MODEL_INTERVAL : return ("Interval"); // XXX implemented differently
            case MODEL_LAT : return ("Latitude");
            case MODEL_LONG : return ("Longitude");
            case MODEL_XYTIME : return ("Time at X/Y");
            case MODEL_POWERZONE : return ("Zone");
        }
        return ("None");; // ? unknown channel ?
    }
}

/*----------------------------------------------------------------------
 * Setup the data model and plot according to the settings passed from
 * mainwindow.
 *
 * Truthfully, this is where all the hard work is done!
 * Edit it with caution, there be dragons here.
 *
 *----------------------------------------------------------------------*/
ModelDataProvider::ModelDataProvider (BasicModelPlot &plot, ModelSettings *settings) : Function(plot)
{
    // if there are no settings or incomplete settings
    // create a null data plot
    if (settings == NULL || settings->ride == NULL ||
        settings->x == 0 || settings->y == 0 || settings->z == 0) {
        // initialise a null plot
        setDomain(0,0,0,0);
        setMinZ(0);
        create();

        return;
    }

    // Run through the ridefile points putting the selected
    // values into the approprate bins
    settings->colorProvider->color.clear();
    settings->colorProvider->num.clear();
    settings->colorProvider->zonecolor.clear();

    //plot.makeCurrent();

    double maxbinx =0, maxbiny =0;
    double minbinx =65535, minbiny =65535;
    double mincol =65535, maxcol =0;

    //
    // Create Plot dataset, filter on values and calculate averages etc
    //
    foreach(const RideFilePoint *point, settings->ride->ride()->dataPoints()) {

        // get x and z bin values - round to nearest bin
        double dx  = pointType(point, settings->x)/settings->xbin;
        int binx = settings->xbin * Qwt3D::round(dx);

        double dy = pointType(point, settings->y)/settings->ybin;
        int biny = settings->ybin * Qwt3D::round(dy);

        // ignore zero points
        if (settings->ignore && (binx==0 || biny==0)) continue;

        // get z value
        double zed=0;
        if (settings->z == MODEL_XYTIME) zed = settings->ride->ride()->recIntSecs(); // time at
        else zed = pointType(point, settings->z); // raw data

        // get color value
        double color=0;
        if (settings->color == MODEL_XYTIME) color = settings->ride->ride()->recIntSecs(); // time at
        else color = pointType(point, settings->color); // raw data

        // min max
        if (color > maxcol) maxcol = color;
        if (color < mincol) mincol = color;
        if (binx > maxbinx) maxbinx = binx;
        if (binx < minbinx) minbinx = binx;
        if (biny > maxbiny) maxbiny = biny;
        if (biny < minbiny) minbiny = biny;

        // How many & Curent Value
        // ZED
        QString lookup = xystring(binx,biny);
        int count = mnum.value(lookup, 0.0);
        double currentz = mz.value(lookup, 0.0);

        if (settings->z == MODEL_XYTIME) {
            count++;
            zed += currentz;
        } else {
           if (count) zed = ((currentz*count)+zed)/(count+1); // average
        }

        // always update the max values
        mz.insert(lookup, zed);
        mnum.insert(lookup, count);

        // NO INTERVALS COLOR IS FOR ALL SAMPLES
        if (settings->intervals.count() == 0 ) {
            intervals_ = 0;

            int colcount = settings->colorProvider->num.value(lookup, 0.0);
            double currentcol = settings->colorProvider->color.value(lookup,0.0);

            if (settings->color == MODEL_XYTIME) { // color in time
                colcount++;
                color += currentcol;
            } else { // color in average of
                if (colcount) color = ((currentcol*colcount)+color)/(colcount+1); // average
            }
            settings->colorProvider->color.insert(lookup, color);
            settings->colorProvider->num.insert(lookup, colcount);
        }

        // WE HAVE INTERVALS! COLOR AND INTERVAL Z VALUES NEED TO BE TREATED
        // DIFFERENTLY NOW - COLOR IS FOR SELECTED INTERVALS AND WE MAINTAIN
        // A SECOND SET OF Z VALUES SO WE HAVE MAX + INTERVALS
        if (settings->intervals.count() > 0) {
            intervals_ = SHOW_INTERVALS;
            if (settings->frame == true) intervals_ |= SHOW_FRAME;

            QString lookup = xystring(binx, biny); // XXX SEGV on QString in multiple hashes!!!! XXX

            // filter for interval
            for(int i=0; i<settings->intervals.count(); i++) {
                IntervalItem *curr = settings->intervals.at(i);
                if (point->secs >= curr->start && point->secs <= curr->stop) {

                    // update colors
                    int colcount = settings->colorProvider->num.value(lookup,0.0);
                    double currentcol = settings->colorProvider->color.value(lookup, 0.0);

                    if (settings->color == MODEL_XYTIME) { // color in time
                        colcount++;
                        color += currentcol;
                    } else { // color in average of
                        if (colcount) color = ((currentcol*colcount)+color)/(colcount+1); // average
                    }
                    settings->colorProvider->color.insert(lookup, color);
                    settings->colorProvider->num.insert(lookup, colcount);

                    double ized=0;
                    if (settings->z == MODEL_XYTIME) ized = settings->ride->ride()->recIntSecs(); // time at
                    else ized = pointType(point, settings->z); // raw data
                    // update interval values
                    int count = inum.value(lookup, 0.0);
                    double currentz = iz.value(lookup, 0.0);

                    if (settings->z == MODEL_XYTIME) {
                        count++;
                        ized += currentz;
                    } else {
                       if (count) ized = ((currentz*count)+ized)/(count+1); // average
                    }

                    iz.insert(lookup, ized);
                    inum.insert(lookup, count);
                    break;
                }
            }
        }
    }

    if (mz.count() == 0) {

        // create a null plot -- bin too large!
        plot.setTitle("No data or bin size too large");
        setDomain(0,0,0,0);
        setMesh(2,2);
        setMinZ(0);
        mz.clear();
        settings->colorProvider->color.clear();
        create();
        return;
    }

    // POST PROCESS DATA SET

    // COLOR
    // if needed convert average power to the related power zone
    // but only if ranges are defined i.e. user has set CP
	const Zones *zones = settings->ride->zones;
	int zone_range = settings->ride->zoneRange();
    if (settings->color == MODEL_POWERZONE && zone_range >= 0) {
        // if we need to color by power zones
        // zones are setup and we want to color by them
        maxcol = settings->colorProvider->max = zones->numZones(zone_range);
        mincol = settings->colorProvider->min = 1;

        // add the zone colors to the color map
        for (int i=0; i<maxcol; i++) {
            settings->colorProvider->zonecolor.insert
                        (i, zoneColor(i, zones->numZones(zone_range)));
        }

        // iterate over the existing power values converting to a power zone
        QHashIterator<QString, double> coli(settings->colorProvider->color);
        while (coli.hasNext()) {
            coli.next();
            QString lookup = coli.key();
            double color = coli.value();
            // turn into power zone
            color = zones->whichZone(zone_range, color);
            // overwrite existing power average with zone number
            // BUT! the zone numbers start at 1 not 0 here to distinguish
            //      between zone 0 and no value at all
            settings->colorProvider->color.insert(lookup, color+1);

        }
        settings->colorProvider->iszones = true;
    } else if (settings->color == MODEL_NONE) {
            settings->colorProvider->iszones = false;
            settings->colorProvider->color.clear();
    } else {
        // otherwise just turn off zoning
        settings->colorProvider->iszones = false;
    }

    // TIME
    //
    // Convert from absolute values to %age of the entire ride

    double duration = settings->ride->ride()->dataPoints().last()->secs +
                       settings->ride->ride()->recIntSecs();

    // Multis...
    if (settings->z == MODEL_XYTIME) {
        // time on Z axis
        QHashIterator<QString, double> zi(mz);
        while (duration && settings->z == MODEL_XYTIME && zi.hasNext()) {
            zi.next();
            double timePercent = (zi.value()/duration) * 100;
            mz.insert(zi.key(), timePercent);
        }
    }
    // Intervals
    if (settings->z == MODEL_XYTIME) {
        // time on Z axis
        QHashIterator<QString, double> ii(iz);
        while (duration && settings->z == MODEL_XYTIME && ii.hasNext()) {
            ii.next();
            double timePercent = (ii.value()/duration) * 100;
            iz.insert(ii.key(), timePercent);
        }
    }

        // time on Color
    if (settings->color == MODEL_XYTIME) {
        mincol=65535; maxcol=0;
        QHashIterator<QString, double> ci(settings->colorProvider->color);
        while (duration && settings->color == MODEL_XYTIME && ci.hasNext()) {
            ci.next();
            double timePercent = (ci.value()/duration) * 100;
            if (timePercent > maxcol) maxcol = timePercent;
            if (timePercent < mincol) mincol = timePercent;
            settings->colorProvider->color.insert(ci.key(), timePercent);
        }
    }

    // MIN and MAX Z (impacts chart geometry)
    // We DO NOT do the same for color since they represent
    // the entire data set and not just the intervals selected (if any)
    bool first = true;
    QHashIterator <QString, double> iz(mz);
    while (iz.hasNext()) {
        double z;
        iz.next();

        z = iz.value();

        if (first == true) {
            minz = maxz = iz.value();
        } else {
            if (z > maxz) maxz = z;
            if (z < minz) minz = z;
        }
        first = false;
    }

    settings->colorProvider->min = mincol;
    settings->colorProvider->max = maxcol;

    //
    // Adjust the plot settings to reflect the data set
    //
    setMinZ(minz);
    setMaxZ(maxz);

    QFont font; // has all the defaults in it

    //
    // Setup the color legend
    //
    if (settings->legend == true && settings->color != MODEL_NONE) {
        plot.showColorLegend(true);
        plot.legend()->setTitleFont(font.family(), 8, QFont::Normal);
        plot.legend()->setOrientation(Qwt3D::ColorLegend::BottomTop, Qwt3D::ColorLegend::Left);
        plot.legend()->setLimits(mincol, maxcol);
        if (settings->colorProvider->iszones == true) {
            plot.legend()->setMajors(maxcol);
            plot.legend()->setMinors(0);
        } else {
            plot.legend()->setMajors(10);
            plot.legend()->setMinors(0);
        }
        plot.legend()->setTitleString(describeType(settings->color, false));
    } else {
        plot.showColorLegend(false);
    }

    // mesh size
    int mx = (maxbinx-minbinx) / settings->xbin;
    int my = (maxbiny-minbiny) / settings->ybin;

    // mesh MUST be at least 2x2 including 0,0
    while (mx < 2) {
        maxbinx += settings->xbin;
        mx++;
    }

    while(my < 2) {
        maxbiny += settings->ybin;
        my++;
    }

    // add some graph paper so the plot is pretty
    if (mx < 4) {
        minbinx -= settings->xbin;
        mx++;
    }

    if (my < 4) {
        minbiny -= settings->ybin;
        my++;
    }

    // mesh is number of bins PLUS ONE (bug in qwtplot3d)
    setMesh(mx + 1, my + 1);

    // domain is max values in each axis
    setDomain(maxbinx,minbinx,minbiny,maxbiny); // max x/y vals

    // set the barsize to a sensible radius (20% space)
    // the +settting->xbin bit is to offset the additional mx/my bug
    double xr = 0.8 * (((double)settings->xbin/((double)(maxbinx-minbinx)+(settings->xbin))) / 2.0);
    double yr = 0.8 * (((double)settings->ybin/((double)(maxbiny-minbiny)+(settings->ybin))) / 2.0);

    diag_ = xr < yr ? (xr*(maxbinx-minbinx)) : (yr*(maxbiny-minbiny));

    // now update the model - before the chart axis/legends
    create();

    double xscale, yscale, zscale;

    if ((maxbinx-minbinx) >= (maxbiny-minbiny) && (maxbinx-minbinx) >= (maxz-minz)) {
        // scale is set off the x-axis
        xscale = 1;
        yscale = (maxbinx-minbinx)/(maxbiny-minbiny);
        zscale = (maxbinx-minbinx)/(maxz-minz);
    } else if ((maxbiny-minbiny) >= (maxbinx-minbinx) && (maxbiny >= minbiny) >= (maxz-minz)) {
        // scale is set off the y-axis
        xscale = (maxbiny-minbiny)/(maxbinx-minbinx);
        yscale = 1;
        zscale = (maxbiny-minbiny)/(maxz-minz);
    } else {
        // scale is set off the z-axis
        xscale = (maxz-minz)/(maxbinx-minbinx);
        yscale = (maxz-minz)/(maxbiny-minbiny);
        zscale = 1;
    }

    // must be integers!?
    if (xscale < 1) {
        double factor = 1 / xscale;
        xscale = 1;
        yscale *= factor;
        zscale *= factor;
    }
    if (yscale < 1) {
        double factor = 1/ yscale;
        yscale = 1;
        xscale *= factor;
        zscale *= factor;
    }
    if (zscale < 1) {
        double factor = 1/ zscale;
        zscale = 1;
        yscale *= factor;
        xscale *=factor;
    }

    plot.setScale(xscale, yscale, zscale);
    plot.setTitle("");
    plot.setCoordinateStyle(FRAME);
    plot.setMeshLineWidth(1);
    plot.coordinates()->setLineWidth(1);
    plot.coordinates()->setNumberFont(font.family(),font.pointSize());
    plot.setTitleFont(font.family(),font.pointSize(), QFont::Bold);

    plot.coordinates()->setLabelFont(font.family(), font.pointSize(), QFont::Bold);
    plot.coordinates()->axes[Z1].setLabelString(describeType(settings->z, true));
    plot.coordinates()->axes[Z2].setLabelString(describeType(settings->z, true));
    plot.coordinates()->axes[Z3].setLabelString(describeType(settings->z, true));
    plot.coordinates()->axes[Z4].setLabelString(describeType(settings->z, true));
    plot.coordinates()->axes[X1].setLabelString(describeType(settings->x, true));
    plot.coordinates()->axes[X2].setLabelString(describeType(settings->x, true));
    plot.coordinates()->axes[X3].setLabelString(describeType(settings->x, true));
    plot.coordinates()->axes[X4].setLabelString(describeType(settings->x, true));
    plot.coordinates()->axes[Y1].setLabelString(describeType(settings->y, true));
    plot.coordinates()->axes[Y2].setLabelString(describeType(settings->y, true));
    plot.coordinates()->axes[Y3].setLabelString(describeType(settings->y, true));
    plot.coordinates()->axes[Y4].setLabelString(describeType(settings->y, true));
    plot.coordinates()->axes[Z1].draw();
    plot.coordinates()->axes[Z2].draw();
    plot.coordinates()->axes[Z3].draw();
    plot.coordinates()->axes[X1].draw();
    plot.coordinates()->axes[X2].draw();
    plot.coordinates()->axes[X3].draw();
    plot.coordinates()->axes[Y1].draw();
    plot.coordinates()->axes[Y2].draw();
    plot.coordinates()->axes[Y3].draw();

    for (unsigned int i=0; i < plot.coordinates()->axes.size(); i++) {
        plot.coordinates()->axes[i].setMajors(7);
        plot.coordinates()->axes[i].setMinors(5);
    }
    plot.setIsolines(10);
    plot.setSmoothMesh(true);
    plot.coordinates()->adjustLabels(diag_*2);
    if (settings->gridlines == true)
        plot.coordinates()->setGridLines(true, true, Qwt3D::BACK | Qwt3D::LEFT | Qwt3D::FLOOR);
    else
        plot.coordinates()->setGridLines(true, true, 0);

    // turn off zpane -- causes nasty flashing when left on between plots
    zpane = 0;

}

/*----------------------------------------------------------------------
 * BASIC MODEL PLOT
 * This is the qwt3d plot object
 *
 * Constructor - initialises an empty plot
 * setData     - calls the model data provider to setup data
 *               and then replots via updateData, updateGL.
 *
 *----------------------------------------------------------------------*/

BasicModelPlot::BasicModelPlot(MainWindow *parent, ModelSettings *settings) : main(parent)
{
    diag_=0;
    currentStyle = STYLE_BAR;

    // the color provider returns a color for an xyz
    modelDataColor = new ModelDataColor;
    if (settings) settings->colorProvider = modelDataColor;

    // the data provider returns a z for an x,y
    modelDataProvider = new ModelDataProvider(*this, settings);
    setDataColor(modelDataColor);

    // box style x/y/z
    setCoordinateStyle(FRAME);

    // start with bar chart
    bar = (Bar *) this->setPlotStyle(Bar());

    //coordinates()->setAutoScale(true);

    // add grid lines and antialias them for smoother lines
    //coordinates()->setGridLines(true, true);
    //coordinates()->setLineSmooth(true);
    //setSmoothMesh(true);

    // some detailed axes points
    for (unsigned int i=0; i < coordinates()->axes.size(); i++) {
        coordinates()->axes[i].setMajors(7);
        coordinates()->axes[i].setMinors(5);
    }
    setMeshLineWidth(1);
    coordinates()->setGridLinesColor(RGBA(0,0,0.5));
    coordinates()->setLineWidth(1);

    // put some space between the axes tic labels and the plot
    // to make it easier to read
    coordinates()->adjustNumbers(25);

    // orthogonal view
    setOrtho(false);

    // no lighting it makes it tricky to read
    // when there are LOTS of bars
    blowout();

    // set shift zoom etc
    resetViewPoint();

    updateData();
    updateGL();
}

void
BasicModelPlot::setStyle(int index)
{
    if (currentStyle == STYLE_BAR) degrade(bar);
    else degrade(water);

    switch (index) {

    case 0 : // BAR
        bar = (Bar*)this->setPlotStyle(Bar());
        showNormals(false);
        updateNormals();
        currentStyle = STYLE_BAR;
        break;
    case 1 : // SURFACE GRID
        setPlotStyle(FILLEDMESH);
        water = (Water *)addEnrichment(Water());
        showNormals(false);
        updateNormals();
        currentStyle = STYLE_GRID;
        break;
    case 2 : // SURFACE SMOOTH
        setPlotStyle(FILLED);
        water = (Water *)addEnrichment(Water());
        showNormals(false);
        updateNormals();
        currentStyle = STYLE_SURFACE;
        break;
    case 3 : // DOTS
        setPlotStyle(Qwt3D::POINTS);
        water = (Water *)addEnrichment(Water());
        showNormals(true);
        updateNormals();
        currentStyle = STYLE_DOTS;
        break;
    }
    updateData();
    updateGL();
}

void
BasicModelPlot::setData(ModelSettings *settings)
{
    delete modelDataProvider;
    settings->colorProvider = modelDataColor;
    modelDataProvider = new ModelDataProvider(*this, settings);
    //modelDataProvider->assign(this);
    //create();
    //resetViewPoint();
    //updateNormals();
    updateData();
    updateGL();
}

void
BasicModelPlot::setFrame(bool frame)
{
    if (intervals_ && frame == true) {
        intervals_ |= SHOW_FRAME;
    } else if (frame== false) {
        intervals_ &= ~SHOW_FRAME;
    }
    updateData();
    updateGL();
}

void
BasicModelPlot::setLegend(bool legend, int coltype)
{
    if (legend == true && coltype != MODEL_NONE) {
        showColorLegend(true);
    } else {
        showColorLegend(false);
    }
}

void
BasicModelPlot::setGrid(bool grid)
{
    if (grid == true)
        coordinates()->setGridLines(true, true, Qwt3D::BACK | Qwt3D::LEFT | Qwt3D::FLOOR);
    else
        coordinates()->setGridLines(true, true, 0);
    updateData();
    updateGL();
}

void
BasicModelPlot::setZPane(int z)
{
    //zpane = (modelDataProvider->maxz-modelDataProvider->minz) / 100 * z;
    zpane = (modelDataProvider->getMaxz()-modelDataProvider->getMinz()) / 100 * z;
    updateData();
    updateGL();
}

void
BasicModelPlot::resetViewPoint()
{
    setRotation(45, 0, 30); // seems most pleasing
    setShift(0,0,0);        // centre so movement feels natural
    setViewportShift(0,0);
    setZoom(0.8); // zoom in close but leave space for the axis labels
}


/*----------------------------------------------------------------------
 * MODEL PLOT
 * Nothing special - just a framed BasicModelPlot
 *----------------------------------------------------------------------*/
ModelPlot::ModelPlot(MainWindow *parent, ModelSettings *settings) : QFrame(parent), main(parent)
{
    // the distinction between a model plot and a basic model plot
    // is only to provide a frame for the qwt3d plot (it looks odd
    // when compared to the other plots without one)
    layout = new QVBoxLayout;
    setLineWidth(1);
    setFrameStyle(QFrame::Box | QFrame::Raised);
    setContentsMargins(0,0,0,0);
    basicModelPlot = new BasicModelPlot(parent, settings);
    layout->addWidget(basicModelPlot);
    layout->setContentsMargins(2,2,2,2);
    setLayout(layout);
}

void
ModelPlot::setStyle(int index)
{
    basicModelPlot->setStyle(index);
}

void
ModelPlot::setResolution(int val)
{
    basicModelPlot->setResolution(val);
}

void
ModelPlot::setData(ModelSettings *settings)
{
    basicModelPlot->setData(settings);
}

void
ModelPlot::resetViewPoint()
{
    basicModelPlot->resetViewPoint();
}

void
ModelPlot::setGrid(bool grid)
{
    basicModelPlot->setGrid(grid);
}

void
ModelPlot::setLegend(bool legend, int coltype)
{
    basicModelPlot->setLegend(legend, coltype);
}

void
ModelPlot::setFrame(bool frame)
{
    basicModelPlot->setFrame(frame);
}

void
ModelPlot::setZPane(int z)
{
    basicModelPlot->setZPane(z);
}


/*----------------------------------------------------------------------
 * WATER VERTEX ENRICHMENT
 *
 * THIS IS *NOT* USED IN BAR STYLE. The water enrichment is done by the
 * Bar ENRICHMENT INSTEAD. (this is because the alpha values are honored
 * amd I cannot work out why).
 *
 * IF we decide to get rid of surface/grid plots then this enrichment can
 * be removed. But maybe someone likes this plot styles. I hate them.
 *
 *----------------------------------------------------------------------*/
Water::Water()
{
}

void Water::drawBegin()
{
  // diag has been moved to a global variable set by the data model
  // this is because the reference variable below is cached
  // and is unreliable!! (i.e. bug).
  //diag_ = (plot->hull().maxVertex-plot->hull().minVertex).length() * barsize;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glLineWidth( 0 );
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(0,1);
}

void Water::drawEnd()
{
    // plot all the nonsense now -- could shade if
    // power zones selected on plot (?)

    if (zpane) {

        double minx = plot->hull().minVertex.x;
        double miny = plot->hull().minVertex.y;
        double maxx = plot->hull().maxVertex.x;
        double maxy = plot->hull().maxVertex.y;
        double minz = plot->hull().minVertex.z;
        double z = zpane + minz;

        // ZPANE SHADING
        glColor4f(0.7,0,0,0.4);
        glBegin(GL_QUADS);

        // top
        glColor4d(0.5,0.5,1,1.0);
        glVertex3d(minx,miny,z);
        glVertex3d(minx,maxy,z);
        glVertex3d(maxx,maxy,z);
        glVertex3d(maxx,miny,z);

        // bottom
        glColor4d(0.5,0.5,1,1.0);
        glVertex3d(minx,miny,minz);
        glVertex3d(minx,maxy,minz);
        glVertex3d(maxx,maxy,minz);
        glVertex3d(maxx,miny,minz);

        // front
        glColor4d(0.5,0.5,1,1.0);
        glVertex3d(minx,miny,minz);
        glVertex3d(minx,miny,z);
        glVertex3d(maxx,miny,z);
        glVertex3d(maxx,miny,minz);

        // back
        glColor4d(0.5,0.5,1,1.0);
        glVertex3d(minx,maxy,minz);
        glVertex3d(minx,maxy,z);
        glVertex3d(maxx,maxy,z);
        glVertex3d(maxx,maxy,minz);

        // left
        glColor4d(0.5,0.5,1,1.0);
        glVertex3d(minx,miny,minz);
        glVertex3d(minx,miny,z);
        glVertex3d(minx,maxy,z);
        glVertex3d(minx,maxy,minz);

        // right
        glColor4d(0.5,0.5,1,1.0);
        glVertex3d(maxx,miny,minz);
        glVertex3d(maxx,miny,z);
        glVertex3d(maxx,maxy,z);
        glVertex3d(maxx,maxy,minz);
        glEnd();

        glColor3d(0,0,0);
        glBegin(GL_LINES);
        glVertex3d(minx,miny,z); glVertex3d(minx,maxy, z);
        glVertex3d(minx,maxy,z); glVertex3d(maxx,maxy, z);
        glVertex3d(maxx,maxy,z); glVertex3d(maxx,miny, z);
        glVertex3d(maxx,miny,z); glVertex3d(minx,miny, z);
        glEnd();
    }
}

// draw the enrichment, called for each xyz triple
void Water::draw(Qwt3D::Triple const& )
{}

/*----------------------------------------------------------------------
 * BAR VERTEX ENRICHMENT (From here to bottom of source file)
 *
 * A vertex enrichment to show bars not a surface
 * Code courtesy of the enrichment example in the qwtplot3d library and
 * hacked to fixup reloading plots etc (the original was a demo example)
 *
 * CHANGED TO DRAW TWO BARS - wireframe for MAX and shaded for interval
 *                            or wireframe and shaded MAX depending upon
 *                            whether any intervals have been selected.
 *
 *----------------------------------------------------------------------*/
Bar::Bar()
{
}

void Bar::drawBegin()
{
  // diag has been moved to a global variable set by the data model
  // this is because the reference variable below is cached
  // and is unreliable!! (i.e. bug).
  //diag_ = (plot->hull().maxVertex-plot->hull().minVertex).length() * barsize;
  glLineWidth( 0 );
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1,1);

}

// enrichment pure virtual - we do nothing
void Bar::drawEnd()
{
    // plot all the nonsense now -- could shade if
    // power zones selected on plot (?)
    if (zpane) {

        double minx = plot->hull().minVertex.x;
        double miny = plot->hull().minVertex.y;
        double maxx = plot->hull().maxVertex.x;
        double maxy = plot->hull().maxVertex.y;
        double minz = plot->hull().minVertex.z;
        double z = zpane + minz;

        // ZPANE SHADING
        glColor3d(0.7,0,0);
        glBegin(GL_QUADS);

        // top
        glColor4d(0.5,0.5,1,0.7);
        glVertex3d(minx,miny,z);
        glVertex3d(minx,maxy,z);
        glVertex3d(maxx,maxy,z);
        glVertex3d(maxx,miny,z);

        // bottom
        glColor4d(0.5,0.5,1,0.7);
        glVertex3d(minx,miny,minz);
        glVertex3d(minx,maxy,minz);
        glVertex3d(maxx,maxy,minz);
        glVertex3d(maxx,miny,minz);

        // front
        glColor4d(0.5,0.5,1,0.7);
        glVertex3d(minx,miny,minz);
        glVertex3d(minx,miny,z);
        glVertex3d(maxx,miny,z);
        glVertex3d(maxx,miny,minz);

        // back
        glColor4d(0.5,0.5,1,0.7);
        glVertex3d(minx,maxy,minz);
        glVertex3d(minx,maxy,z);
        glVertex3d(maxx,maxy,z);
        glVertex3d(maxx,maxy,minz);

        // left
        glColor4d(0.5,0.5,1,0.7);
        glVertex3d(minx,miny,minz);
        glVertex3d(minx,miny,z);
        glVertex3d(minx,maxy,z);
        glVertex3d(minx,maxy,minz);

        // left
        glColor4d(0.5,0.5,1,0.7);
        glVertex3d(maxx,miny,minz);
        glVertex3d(maxx,miny,z);
        glVertex3d(maxx,maxy,z);
        glVertex3d(maxx,maxy,minz);
        glEnd();

        glColor3d(0,0,0);
        glBegin(GL_LINES);
        glVertex3d(minx,miny,z); glVertex3d(minx,maxy, z);
        glVertex3d(minx,maxy,z); glVertex3d(maxx,maxy, z);
        glVertex3d(maxx,maxy,z); glVertex3d(maxx,miny, z);
        glVertex3d(maxx,miny,z); glVertex3d(minx,miny, z);
        glEnd();
    }
}

// draw the enrichment, called for each xyz triple
void Bar::draw(Qwt3D::Triple const& pos)
{

    //  GLStateBewarer sb(GL_LINE_SMOOTH, true);
    //  sb.turnOn();

    double interval = plot->hull().maxVertex.z-plot->hull().minVertex.z;
    double numlevel = plot->hull().minVertex.z + 1 * interval;
    interval /=100;

    GLdouble gminz = plot->hull().minVertex.z;

    // get the colour for this bar from the plot colour provider (defined above)
    RGBA rgbat, rgbab;

    // don't plot a thing if it don't mean a thing
    if (pos.z == gminz) return;
    if (intervals_ == 0) {
        // just plot using normal colours (one set of bars per x/y)
        rgbat = (*plot->dataColor())(pos);
        rgbab = (*plot->dataColor())(pos.x, pos.y, gminz);
    } else {
        // first bars use max and are see-through
        rgbat = RGBA(255,255,255,1);
        rgbab = RGBA(255,255,255);
    }

    if (intervals_ == 0) {
        // shade the max bars if not doing intervals
        glBegin(GL_QUADS);
        glColor4d(rgbab.r,rgbab.g,rgbab.b,rgbab.a);
        glVertex3d(pos.x-diag_,pos.y-diag_,gminz);
        glVertex3d(pos.x+diag_,pos.y-diag_,gminz);
        glVertex3d(pos.x+diag_,pos.y+diag_,gminz);
        glVertex3d(pos.x-diag_,pos.y+diag_,gminz);

        if (pos.z > numlevel - interval && pos.z < numlevel + interval )
          glColor3d(0.7,0,0);
        else
          glColor4d(rgbat.r,rgbat.g,rgbat.b,rgbat.a);
        glVertex3d(pos.x-diag_,pos.y-diag_,pos.z);
        glVertex3d(pos.x+diag_,pos.y-diag_,pos.z);
        glVertex3d(pos.x+diag_,pos.y+diag_,pos.z);
        glVertex3d(pos.x-diag_,pos.y+diag_,pos.z);

        glColor4d(rgbab.r,rgbab.g,rgbat.b,rgbab.a);
        glVertex3d(pos.x-diag_,pos.y-diag_,gminz);
        glVertex3d(pos.x+diag_,pos.y-diag_,gminz);
        glColor4d(rgbat.r,rgbat.g,rgbat.b,rgbat.a);
        glVertex3d(pos.x+diag_,pos.y-diag_,pos.z);
        glVertex3d(pos.x-diag_,pos.y-diag_,pos.z);

        glColor4d(rgbab.r,rgbab.g,rgbat.b,rgbab.a);
        glVertex3d(pos.x-diag_,pos.y+diag_,gminz);
        glVertex3d(pos.x+diag_,pos.y+diag_,gminz);
        glColor4d(rgbat.r,rgbat.g,rgbat.b,rgbat.a);
        glVertex3d(pos.x+diag_,pos.y+diag_,pos.z);
        glVertex3d(pos.x-diag_,pos.y+diag_,pos.z);

        glColor4d(rgbab.r,rgbab.g,rgbat.b,rgbab.a);
        glVertex3d(pos.x-diag_,pos.y-diag_,gminz);
        glVertex3d(pos.x-diag_,pos.y+diag_,gminz);
        glColor4d(rgbat.r,rgbat.g,rgbat.b,rgbat.a);
        glVertex3d(pos.x-diag_,pos.y+diag_,pos.z);
        glVertex3d(pos.x-diag_,pos.y-diag_,pos.z);

        glColor4d(rgbab.r,rgbab.g,rgbat.b,rgbab.a);
        glVertex3d(pos.x+diag_,pos.y-diag_,gminz);
        glVertex3d(pos.x+diag_,pos.y+diag_,gminz);
        glColor4d(rgbat.r,rgbat.g,rgbat.b,rgbat.a);
        glVertex3d(pos.x+diag_,pos.y+diag_,pos.z);
        glVertex3d(pos.x+diag_,pos.y-diag_,pos.z);
        glEnd();
    }

    if (intervals_ == 0 || intervals_&SHOW_FRAME) {
        glColor3d(0,0,0);
        glBegin(GL_LINES);
        glVertex3d(pos.x-diag_,pos.y-diag_,gminz); glVertex3d(pos.x+diag_,pos.y-diag_,gminz);
        glVertex3d(pos.x-diag_,pos.y-diag_,pos.z); glVertex3d(pos.x+diag_,pos.y-diag_,pos.z);
        glVertex3d(pos.x-diag_,pos.y+diag_,pos.z); glVertex3d(pos.x+diag_,pos.y+diag_,pos.z);
        glVertex3d(pos.x-diag_,pos.y+diag_,gminz); glVertex3d(pos.x+diag_,pos.y+diag_,gminz);

        glVertex3d(pos.x-diag_,pos.y-diag_,gminz); glVertex3d(pos.x-diag_,pos.y+diag_,gminz);
        glVertex3d(pos.x+diag_,pos.y-diag_,gminz); glVertex3d(pos.x+diag_,pos.y+diag_,gminz);
        glVertex3d(pos.x+diag_,pos.y-diag_,pos.z); glVertex3d(pos.x+diag_,pos.y+diag_,pos.z);
        glVertex3d(pos.x-diag_,pos.y-diag_,pos.z); glVertex3d(pos.x-diag_,pos.y+diag_,pos.z);

        glVertex3d(pos.x-diag_,pos.y-diag_,gminz); glVertex3d(pos.x-diag_,pos.y-diag_,pos.z);
        glVertex3d(pos.x+diag_,pos.y-diag_,gminz); glVertex3d(pos.x+diag_,pos.y-diag_,pos.z);
        glVertex3d(pos.x+diag_,pos.y+diag_,gminz); glVertex3d(pos.x+diag_,pos.y+diag_,pos.z);
        glVertex3d(pos.x-diag_,pos.y+diag_,gminz); glVertex3d(pos.x-diag_,pos.y+diag_,pos.z);
        glEnd();
    }

    // if we have don't have intervals we're done
    if (intervals_ == 0) return;

    // get the intervals drawn
    // just plot using normal colours (one set of bars per x/y)
    rgbat = (*plot->dataColor())(pos);
    rgbab = (*plot->dataColor())(pos.x, pos.y, gminz);

    // get pos for the interval data
    // call the current data provider
    // which is a global
    double z =  iz.value(xystring(pos.x,pos.y));
    if (z == 0) return;

    // do the max bars
    glBegin(GL_QUADS);
    glColor4d(rgbab.r,rgbab.g,rgbab.b,rgbab.a);
    glVertex3d(pos.x-diag_,pos.y-diag_,gminz);
    glVertex3d(pos.x+diag_,pos.y-diag_,gminz);
    glVertex3d(pos.x+diag_,pos.y+diag_,gminz);
    glVertex3d(pos.x-diag_,pos.y+diag_,gminz);

    if (z > numlevel - interval && z < numlevel + interval )
      glColor3d(0.7,0,0);
    else
      glColor4d(rgbat.r,rgbat.g,rgbat.b,rgbat.a);
    glVertex3d(pos.x-diag_,pos.y-diag_,z);
    glVertex3d(pos.x+diag_,pos.y-diag_,z);
    glVertex3d(pos.x+diag_,pos.y+diag_,z);
    glVertex3d(pos.x-diag_,pos.y+diag_,z);

    glColor4d(rgbab.r,rgbab.g,rgbat.b,rgbab.a);
    glVertex3d(pos.x-diag_,pos.y-diag_,gminz);
    glVertex3d(pos.x+diag_,pos.y-diag_,gminz);
    glColor4d(rgbat.r,rgbat.g,rgbat.b,rgbat.a);
    glVertex3d(pos.x+diag_,pos.y-diag_,z);
    glVertex3d(pos.x-diag_,pos.y-diag_,z);

    glColor4d(rgbab.r,rgbab.g,rgbat.b,rgbab.a);
    glVertex3d(pos.x-diag_,pos.y+diag_,gminz);
    glVertex3d(pos.x+diag_,pos.y+diag_,gminz);
    glColor4d(rgbat.r,rgbat.g,rgbat.b,rgbat.a);
    glVertex3d(pos.x+diag_,pos.y+diag_,z);
    glVertex3d(pos.x-diag_,pos.y+diag_,z);

    glColor4d(rgbab.r,rgbab.g,rgbat.b,rgbab.a);
    glVertex3d(pos.x-diag_,pos.y-diag_,gminz);
    glVertex3d(pos.x-diag_,pos.y+diag_,gminz);
    glColor4d(rgbat.r,rgbat.g,rgbat.b,rgbat.a);
    glVertex3d(pos.x-diag_,pos.y+diag_,z);
    glVertex3d(pos.x-diag_,pos.y-diag_,z);

    glColor4d(rgbab.r,rgbab.g,rgbat.b,rgbab.a);
    glVertex3d(pos.x+diag_,pos.y-diag_,gminz);
    glVertex3d(pos.x+diag_,pos.y+diag_,gminz);
    glColor4d(rgbat.r,rgbat.g,rgbat.b,rgbat.a);
    glVertex3d(pos.x+diag_,pos.y+diag_,z);
    glVertex3d(pos.x+diag_,pos.y-diag_,z);
    glEnd();

    glColor3d(0,0,0);
    glBegin(GL_LINES);
    glVertex3d(pos.x-diag_,pos.y-diag_,gminz); glVertex3d(pos.x+diag_,pos.y-diag_,gminz);
    glVertex3d(pos.x-diag_,pos.y-diag_,z); glVertex3d(pos.x+diag_,pos.y-diag_,z);
    glVertex3d(pos.x-diag_,pos.y+diag_,z); glVertex3d(pos.x+diag_,pos.y+diag_,z);
    glVertex3d(pos.x-diag_,pos.y+diag_,gminz); glVertex3d(pos.x+diag_,pos.y+diag_,gminz);

    glVertex3d(pos.x-diag_,pos.y-diag_,gminz); glVertex3d(pos.x-diag_,pos.y+diag_,gminz);
    glVertex3d(pos.x+diag_,pos.y-diag_,gminz); glVertex3d(pos.x+diag_,pos.y+diag_,gminz);
    glVertex3d(pos.x+diag_,pos.y-diag_,z); glVertex3d(pos.x+diag_,pos.y+diag_,z);
    glVertex3d(pos.x-diag_,pos.y-diag_,z); glVertex3d(pos.x-diag_,pos.y+diag_,z);

    glVertex3d(pos.x-diag_,pos.y-diag_,gminz); glVertex3d(pos.x-diag_,pos.y-diag_,z);
    glVertex3d(pos.x+diag_,pos.y-diag_,gminz); glVertex3d(pos.x+diag_,pos.y-diag_,z);
    glVertex3d(pos.x+diag_,pos.y+diag_,gminz); glVertex3d(pos.x+diag_,pos.y+diag_,z);
    glVertex3d(pos.x-diag_,pos.y+diag_,gminz); glVertex3d(pos.x-diag_,pos.y+diag_,z);
    glEnd();
}