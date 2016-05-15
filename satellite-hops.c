#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>

#define EARTH_RADIUS 6371
#define CSV_SEPARATOR ","
#define SATELLITES_COUNT 20

struct LatLon {
	float latitude;
	float longitude;
};

struct Coordinates {
	float x;
	float y;
	float z;
};

struct Satellite {
	char *id;
	struct LatLon latlon;
	struct Coordinates coords;
	float altitude;
};

char *filename;
float seed;
struct Coordinates *start;
struct Coordinates *end;
struct Satellite **satellites;

void die(const char *message)
{
    if(errno) {
        perror(message);
    } else {
        printf("Error: %s\n", message);
    }

    exit(1);
}

// http://stackoverflow.com/a/1185413
struct Coordinates *getCoordinatesFromLatLon(float lat, float lon, float alt)
{
	struct Coordinates *coords = malloc(sizeof(struct Coordinates));
	if (!coords) {
		die("Memory error");
	}

	float radius = EARTH_RADIUS + alt;

	coords->x = radius * cos(lat) * cos(lon);
	coords->y = radius * cos(lat) * sin(lon);
	coords->z = radius * sin(lat);

	return coords;
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

void setStart(float lat, float lon)
{
	printf("%.5f, %.5f\n", lat, lon);
	start = malloc(sizeof(struct Coordinates));

	if (!start) {
		die("Memory error");
	}

	start = getCoordinatesFromLatLon(lat, lon, 0);
}

void setEnd(float lat, float lon)
{
	end = malloc(sizeof(struct Coordinates));

	if (!end) {
		die("Memory error");
	}

	end = getCoordinatesFromLatLon(lat, lon, 0);
}

void addSatellite(char *id, float lat, float lon, float alt)
{
	struct Satellite *sat = malloc(sizeof(struct Satellite));

	if (!sat) {
		die("Memory error");
	}

	sat->id = id;
	sat->latlon = malloc(sizeof(struct LatLon));

	if (!sat->latlon) {
		die("Memory error");
	}

	sat->latlon->latitude = lat;
	sat->latlon->longitude = lon;
	sat->altitude = alt;
	sat->coords = getCoordinatesFromLatLon(lat, lon, alt);
}

void readDataFile()
{
	FILE *stream = fopen(filename, "r");

	if (!stream) {
		die("Error: File not found");
	}

	char line[1024];
	int i = 0;

	while (fgets(line, 1024, stream)) {
		char *tmp = strdup(line);

		if (i == 0) {
			// First row is seed row, which has prefix "#SEED: "
			seed = atof(tmp + 7);
		} else if (tmp[0] == 'R') {
			// Route row.
			char **lineFields = getCsvLineFields(line);
			setStart(atof(lineFields[1]), atof(lineFields[2]));
			setEnd(atof(lineFields[3]), atof(lineFields[4]));
		} else if (tmp[0] == 'S') {
			// Satellite row.
		}

		i++;
		free(tmp);
	}
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		die("Usage: ./satellite-hops datafile.csv");
	}

	filename = argv[1];
	satellites = malloc(SATELLITES_COUNT * sizeof(struct Satellite));

	if (!satellites) {
		die("Memory error");
	}

	readDataFile();

	printf("%.6f\n", start->x);

	return 0;
}
