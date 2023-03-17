/*
 * geocoord.c
 *
 ******************************************************************************
  This file contains routines that can be bound to a Postgres backend and
  called by the backend in the process of processing queries.  The calling
  format for these routines is dictated by Postgres architecture.
******************************************************************************/

#include "postgres.h"
#include "fmgr.h"
#include <stdbool.h>
#include <string.h>

PG_MODULE_MAGIC;

typedef struct GeoCoord
{
	float		Value1;
	float		Value2;
	char		ValueA;
	char		ValueB;
	char		Space;
	bool		Lat_F;
	char*		CityName;
}			GeoCoord;


/*****************************************************************************
 * Input/Output functions
 *****************************************************************************/

PG_FUNCTION_INFO_V1(geocoord_in);

Datum
geocoord_in(PG_FUNCTION_ARGS)
{
	char	   *str = PG_GETARG_CSTRING(0);
	float		v1,
				v2;
	char		va,
				vb;
	char		space;
	bool		lat_f;
	char*		cn;
	GeoCoord    *result;

	if (sscanf(str, "'%s,%lf°%c%c%lf°%c'",&cn, &v1, &va, &space, &v2, &vb) != 6)
		error:
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for type %s: \"%s\"",
						"GeoCoord", str)));

	if ( !(0 <= v1 && 0 <= v2 && v1 < 90 && v2 < 180))
	{
		goto error;
	}

	for(int i = 0; cn[i] != '\0'; i++) {
        if(!isalpha(str[i]) && !isspace(str[i])) {
            goto error;
        }
    }

	if (va == 'N' || va == 'S')
	{
		lat_f = true;
		if (vb == 'W' || vb == 'E')
		{
			goto error;
		}
		
	}else if (va == 'W' || va == 'E')
	{
		lat_f = false;
		if (vb == 'N' || vb == 'S')
		{
			goto error;
		}
		
	}else{
		goto error;
	}
	

	result = (GeoCoord *) palloc(sizeof(GeoCoord));
	if (lat_f)
	{
		result->Value1 = v1;
		result->Value2 = v2;
		result->ValueA = va;
		result->ValueB = vb;
	}else
	{
		result->Value1 = v2;
		result->Value2 = v1;
		result->ValueA = vb;
		result->ValueB = va;
	}
	result->Space = space;
	result->Lat_F = lat_f;
	result->CityName = cn;
	PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(geocoord_out);

Datum
geocoord_out(PG_FUNCTION_ARGS)
{
	GeoCoord    *geocoord = (GeoCoord *) PG_GETARG_POINTER(0);
	char	   *result;

	if (geocoord->Lat_F)
	{
		result = psprintf("'%s,%g°%c%c%g°%c'", geocoord->CityName,geocoord->Value1,geocoord->ValueA,geocoord->Value2,geocoord->ValueB);
	}else
	{
		result = psprintf("'%s,%g°%c%c%g°%c'", geocoord->CityName,geocoord->Value2,geocoord->ValueB,geocoord->Value1,geocoord->ValueA);
	}
	PG_RETURN_CSTRING(result);
}

/*****************************************************************************
 * New Operators
 *
 * A practical Complex datatype would provide much more than this, of course.
 *****************************************************************************/


bool geocoord_equal(GeoCoord *a,GeoCoord *b)
{
	bool cne = strcasecmp(a->CityName,b->CityName);
	bool ve = (a->Value1 == b->Value1)&&(a->Value2==b->Value2);
	bool ee = (a->ValueA == b->ValueA)&&(a->ValueB==b->ValueB);

	PG_RETURN_BOOL(cne && ve && ee);
}

bool geocoord_compare(GeoCoord* a,GeoCoord* b)
{
	bool result = false;

	if (a->Value1 > b->Value1)
	{
		result = true;
		return result;
	}else if(a->Value1 == b->Value1)
	{
		if (a->ValueA == 'N' && b->ValueA == 'S')
		{
			result = true;
			return result;
		}
	}
	
	if (a->Value2 > b->Value2)
	{
		result = true;
		return result;
	}else if(a->Value2 == b->Value2)
	{
		if (a->ValueB == 'E' && b->ValueB == 'W')
		{
			result = true;
			return result;
		}
	}
	
	result = strcmp(a->CityName,b->CityName) > 0

	return result;
}

bool geocoord_equal_zone(GeoCoord* a,GeoCoord* b)
{
	bool result = false;

	if (a->Value2/15 == b->Value2/15 && a->ValueB==b->ValueB)
	{
		result = true;
	}

	return result;
}

char* toDMS(float coord)
{
	int degrees = (int)coord;
    double minutes_raw = (coord - degrees) * 60;
    int minutes = (int)minutes_raw;
    double seconds = (minutes_raw - minutes) * 60;

	char* str = psprintf("%d°",degrees);

	if (minutes != 0)
	{
		str = psprintf("%s%d'",str,minutes);
	}
	if (seconds != 0)
	{
		str = psprintf("%s%d\"",str,seconds);
	}
	
	return str;
}

PG_FUNCTION_INFO_V1(Convert2DMS);

Datum
Convert2DMS(PG_FUNCTION_ARGS)
{
	GeoCoord    *a = (GeoCoord *) PG_GETARG_POINTER(0);
	char* result;
	if (a->Lat_F)
	{
		result = psprintf("%s,%s%c%c%s%c",a->CityName,toDMS(a->Value1),a->ValueA,toDMS(a->Value2),a->ValueB);
	}else{
		result = psprintf("%s,%s%c%c%s%c",a->CityName,toDMS(a->Value2),a->ValueB,toDMS(a->Value1),a->ValueA);
	}
	
	PG_RETURN_CSTRING(result);
}

PG_FUNCTION_INFO_V1(geocoord_compare_l);

Datum
geocoord_compare_l(PG_FUNCTION_ARGS)
{
	GeoCoord    *a = (GeoCoord *) PG_GETARG_POINTER(0);
	GeoCoord    *b = (GeoCoord *) PG_GETARG_POINTER(1);
	
	bool result = geocoord_compare(a,b);

	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(geocoord_compare_le);

Datum
geocoord_compare_le(PG_FUNCTION_ARGS)
{
	GeoCoord    *a = (GeoCoord *) PG_GETARG_POINTER(0);
	GeoCoord    *b = (GeoCoord *) PG_GETARG_POINTER(1);
	
	bool result = !geocoord_compare(b,a);

	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(geocoord_compare_e);

Datum
geocoord_compare_e(PG_FUNCTION_ARGS)
{
	GeoCoord    *a = (GeoCoord *) PG_GETARG_POINTER(0);
	GeoCoord    *b = (GeoCoord *) PG_GETARG_POINTER(1);
	
	bool result = geocoord_equal(a,b);

	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(geocoord_compare_re);

Datum
geocoord_compare_re(PG_FUNCTION_ARGS)
{
	GeoCoord    *a = (GeoCoord *) PG_GETARG_POINTER(0);
	GeoCoord    *b = (GeoCoord *) PG_GETARG_POINTER(1);
	
	bool result = !geocoord_compare(a,b);

	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(geocoord_compare_r);

Datum
geocoord_compare_r(PG_FUNCTION_ARGS)
{
	GeoCoord    *a = (GeoCoord *) PG_GETARG_POINTER(0);
	GeoCoord    *b = (GeoCoord *) PG_GETARG_POINTER(1);
	
	bool result = geocoord_compare(b,a);

	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(geocoord_e_zone);

Datum
geocoord_e_zone(PG_FUNCTION_ARGS)
{
	GeoCoord    *a = (GeoCoord *) PG_GETARG_POINTER(0);
	GeoCoord    *b = (GeoCoord *) PG_GETARG_POINTER(1);
	
	bool result = geocoord_equal_zone(a,b);

	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(geocoord_not_e_zone);

Datum
geocoord_not_e_zone(PG_FUNCTION_ARGS)
{
	GeoCoord    *a = (GeoCoord *) PG_GETARG_POINTER(0);
	GeoCoord    *b = (GeoCoord *) PG_GETARG_POINTER(1);
	
	bool result = !geocoord_equal_zone(a,b);

	PG_RETURN_BOOL(result);
}