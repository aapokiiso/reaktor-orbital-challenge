#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>

#define EARTH_RADIUS 6371
#define CSV_SEPARATOR ","
#define SATELLITES_COUNT 20
#define LOCATIONS_COUNT 22
#define START_ID "START"
#define END_ID "END"

struct LatLon {
    float latitude;
    float longitude;
};

struct Coordinates {
    float x;
    float y;
    float z;
};

struct Location {
    char *id;
    struct LatLon *latlon;
    struct Coordinates *coords;
    float altitude;
};

char *filename;
float seed;
struct Location *start;
struct Location *end;
struct Location **satellites;
struct Location **route;
int satellites_index, route_index;

void die(const char *message)
{
    if(errno) {
        perror(message);
    } else {
        printf("Error: %s\n", message);
    }

    exit(1);
}

void cleanup()
{
    free(start);
    free(end);
    free(satellites);
    free(route);
}

void printRoute()
{
    int i = 0;

    printf("Satellite hops:\n");
    while (i < route_index) {
        printf("%s", route[i]->id);
        if (i < route_index - 1) {
            printf(",");
        }
        i++;
    }
    printf("\n");
}

// http://stackoverflow.com/a/1185413
struct Coordinates *getCoordinatesFromLatLon(float lat, float lon, float alt)
{
    struct Coordinates *coords = malloc(sizeof(struct Coordinates));
    if (!coords) {
        die("Memory error");
    }

    float radius = EARTH_RADIUS;
    float cosLat = cos(lat * M_PI / 180.0);
    float sinLat = sin(lat * M_PI / 180.0);
    float cosLon = cos(lon * M_PI / 180.0);
    float sinLon = sin(lon * M_PI / 180.0);

    coords->x = radius * cosLat * cosLon;
    coords->y = radius * cosLat * sinLon;
    coords->z = radius * sinLat;

    return coords;
}

// https://en.wikipedia.org/wiki/Horizon#Exact_formula_for_a_spherical_Earth
float getDistanceToHorizon(altitude)
{
    return sqrt(2 * EARTH_RADIUS * altitude + pow(altitude, 2));
}

// https://www.easycalculation.com/analytical/learn-distance3.php
float getDistanceBetweenCoordinates(struct Coordinates *coords1, struct Coordinates *coords2)
{
    return sqrt(pow(coords2->x - coords1->x, 2) + pow(coords2->y - coords1->y, 2) + pow(coords2->z - coords1->z, 2));
}

// If distance between coordinates is less than their combined
// distances to the horizon, they are in line of sight of each other.
//
// Easiest way to visualize is with the following image:
// https://upload.wikimedia.org/wikipedia/commons/2/21/GeometricDistanceToHorizon.png
// If you imagine the point O was mirrored to the other side of point H,
// both of their distances to the horizon would be d, and the distance
// between O and O would be d + d. At this point they are just in the line of sight
// of each other. Now if you move either of the O points "lower" along
// the curvature of the Earth (without changing altitude), the distance between
// the points increases while the distance to the horizon of both points stay the same.
// Thus the distance between the points is larger than the points' combined distances to the
// horizon, and they do not have direct line of sight anymore.
int isLineOfSight(struct Location *loc1, struct Location *loc2)
{
    float distance = getDistanceBetweenCoordinates(loc1->coords, loc2->coords);
    float horizon1 = getDistanceToHorizon(loc1->altitude);
    float horizon2 = getDistanceToHorizon(loc2->altitude);

    return distance <= horizon1 + horizon2;
}

void addLocation(char *id, float lat, float lon, float alt)
{
    struct Location *location = malloc(sizeof(struct Location));

    if (!location) {
        die("Memory error");
    }

    location->id = id;
    location->latlon = malloc(sizeof(struct LatLon));

    if (!location->latlon) {
        die("Memory error");
    }

    location->latlon->latitude = lat;
    location->latlon->longitude = lon;
    location->altitude = alt;
    location->coords = getCoordinatesFromLatLon(lat, lon, alt);

    if (strcmp(location->id, START_ID) == 0) {
        start = location;
    } else if (strcmp(location->id, END_ID) == 0) {
        end = location;
    } else {
        satellites[satellites_index] = location;
        satellites_index++;
    }
}

void addLocationToRoute(struct Location *loc)
{
    route[route_index] = loc;
    route_index++;
}

char **getCsvLineFields(char *line)
{
    char *input = strdup(line);
    char **fields = malloc(10 * sizeof(char *));
    char **fp;

    if (!input || !fields) {
        die("Memory error");
    }

    // Set field pointer (fp) to current field (fields)
    for (fp = fields; (*fp = strsep(&input, ",")) != NULL;) {
        // First check that first char of current field isn't null terminate.
        if (**fp != '\0') {
            // If overflowing max field count (10), break out of loop.
            if (++fp >= &fields[10]) {
                break;
            }
        }
    }

    return fields;
}

void readDataFile()
{
    FILE *fp = fopen(filename, "r");

    if (!fp) {
        die("Error: File not found");
    }

    char line[1024];
    int i = 0;

    while (fgets(line, 1024, fp)) {
        char *tmp = strdup(line);
        char **lineFields = getCsvLineFields(line);

        if (i == 0) {
            // First row is seed row, which has prefix "#SEED: "
            seed = atof(tmp + 7);
        } else if (tmp[0] == 'R') {
            // Route row.
            addLocation(START_ID, atof(lineFields[1]), atof(lineFields[2]), 0.0);
            addLocation(END_ID, atof(lineFields[3]), atof(lineFields[4]), 0.0);
        } else if (tmp[0] == 'S') {
            // Satellite row.
            addLocation(lineFields[0], atof(lineFields[1]), atof(lineFields[2]), atof(lineFields[3]));
        }

        i++;
        free(tmp);
    }

    fclose(fp);
}

int findNextLocation()
{
    // Current location is in route's memory address.
    struct Location *cloc = route[route_index - 1];
    int routeComplete = 0;

    if (isLineOfSight(cloc, end) || route_index >= SATELLITES_COUNT - 1) {
        addLocationToRoute(end);
        routeComplete = 1;
    }

    for (int i = 0; i < SATELLITES_COUNT && !routeComplete;i++) {
        // Skip current satellite from satellites to check.
        if (satellites[i] == cloc) {
            continue;
        }

        // Make sure satellite hasn't been added to route,
        // otherwise will go back and forth between 1st and 2nd satellite.
        int isInRoute = 0;
        for (int o = 0; o < LOCATIONS_COUNT; o++) {
            if (route[o] == satellites[i]) {
                isInRoute = 1;
            }
        }

        if (!isInRoute && isLineOfSight(cloc, satellites[i])) {
            addLocationToRoute(satellites[i]);
              routeComplete = findNextLocation();
        }
    }

    return routeComplete;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        die("Usage: ./satellite-hops datafile.csv");
    }

    filename = argv[1];
    satellites = malloc(SATELLITES_COUNT * sizeof(struct Location));
    route = malloc(LOCATIONS_COUNT * sizeof(struct Location));
    satellites_index = 0;
    route_index = 0;

    if (!satellites || !route) {
        die("Memory error");
    }

    readDataFile();

    addLocationToRoute(start);
    findNextLocation();
    printRoute();

    cleanup();

    return 0;
}
