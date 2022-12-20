/* contrib/pg_debug/pg_debug--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_debug" to load this file. \quit

CREATE FUNCTION sql_info_reset()
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C PARALLEL SAFE;

CREATE FUNCTION sql_info(IN id int4,
    OUT sql_id int4,
    OUT sql_str text,
    OUT parse_tree text,
    OUT rewritten_parse_tree text,
    OUT plan text
)
RETURNS SETOF record
AS 'MODULE_PATHNAME', 'sql_info'
LANGUAGE C STRICT VOLATILE PARALLEL SAFE;

CREATE VIEW sql_info AS
    SELECT * FROM sql_info(0);

GRANT SELECT ON sql_info TO PUBLIC;
