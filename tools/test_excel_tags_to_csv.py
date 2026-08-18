"""Tests for excel_tags_to_csv.py — written before the implementation (TDD)."""

import csv
import io

import openpyxl
import pytest

from excel_tags_to_csv import (
    MappingError,
    OUTPUT_COLUMNS,
    RowError,
    build_row,
    convert_workbook,
    parse_modicon_address,
    sanitize_text,
    translate_format,
    validate_mapping_against_headers,
    write_csv,
)


# --- parse_modicon_address -------------------------------------------------

@pytest.mark.parametrize(
    "raw,expected_type,expected_address",
    [
        ("00001", "Coil", 0),
        ("10001", "DiscreteInput", 0),
        ("30001", "InputRegister", 0),
        ("40001", "HoldingRegister", 0),
        ("40011", "HoldingRegister", 10),
        (40001, "HoldingRegister", 0),
        (40001.0, "HoldingRegister", 0),
    ],
)
def test_parse_modicon_address_valid(raw, expected_type, expected_address):
    assert parse_modicon_address(raw) == (expected_type, expected_address)


def test_parse_modicon_address_rejects_wrong_length():
    with pytest.raises(RowError):
        parse_modicon_address("4001")


def test_parse_modicon_address_rejects_unknown_prefix():
    with pytest.raises(RowError):
        parse_modicon_address("20001")


def test_parse_modicon_address_rejects_non_numeric():
    with pytest.raises(RowError):
        parse_modicon_address("ABCDE")


def test_parse_modicon_address_rejects_non_integer_float():
    with pytest.raises(RowError):
        parse_modicon_address(40001.5)


# --- translate_format --------------------------------------------------

DATA_TYPE_MAP = {
    "uint16": "UnsignedDecimal",
    "uint": "UnsignedDecimal",
    "int": "SignedDecimal",
    "float": "Float32",
    "real32": "Float32",
    "real": "Float32",
    "onoffboolean": "BOOL",
}


@pytest.mark.parametrize(
    "raw,expected",
    [
        ("uint16", "UnsignedDecimal"),
        ("UInt", "UnsignedDecimal"),
        ("int", "SignedDecimal"),
        ("float", "Float32"),
        ("REAL32", "Float32"),
        ("real", "Float32"),
        (" uint16 ", "UnsignedDecimal"),
    ],
)
def test_translate_format_known_types(raw, expected):
    assert translate_format(raw, DATA_TYPE_MAP) == expected


def test_translate_format_bool_sentinel_returns_blank():
    assert translate_format("onoffboolean", DATA_TYPE_MAP) == ""


def test_translate_format_unknown_raises():
    with pytest.raises(RowError):
        translate_format("weird_type", DATA_TYPE_MAP)


# --- sanitize_text -------------------------------------------------------

def test_sanitize_text_replaces_comma():
    assert sanitize_text("Tank, North") == "Tank; North"


def test_sanitize_text_handles_none():
    assert sanitize_text(None) == ""


def test_sanitize_text_strips_whitespace():
    assert sanitize_text("  Tank  ") == "Tank"


# --- validate_mapping_against_headers -------------------------------------

def test_validate_mapping_against_headers_passes_when_all_present():
    validate_mapping_against_headers(
        ["Label", "Address", "Data Type", "Unit"],
        {"Label": "label", "Address": "address"},
        "Sheet1",
    )


def test_validate_mapping_against_headers_raises_when_missing():
    with pytest.raises(MappingError, match="Sheet1"):
        validate_mapping_against_headers(
            ["Label", "Data Type", "Unit"],
            {"Label": "label", "Address": "address"},
            "Sheet1",
        )


# --- build_row -------------------------------------------------------------

SHEET1_COLUMNS = {"Label": "label", "Address": "address", "Data Type": "format", "Unit": "unit"}
SHEET2_COLUMNS = {"Label": "label", "Tag Address": "address", "Data Type": "format", "Unit": "unit"}
DEFAULTS = {"byteOrder": "ABCD", "scale": 1, "offset": 0}


def test_build_row_sheet1_style():
    row = build_row(
        {"Label": "Tank Level", "Address": "40001", "Data Type": "float", "Unit": "m", "Parameter Id": "P1"},
        SHEET1_COLUMNS,
        DATA_TYPE_MAP,
        DEFAULTS,
    )
    assert row["label"] == "Tank Level"
    assert row["registerType"] == "HoldingRegister"
    assert row["address"] == 0
    assert row["format"] == "Float32"
    assert row["byteOrder"] == "ABCD"
    assert row["scale"] == 1
    assert row["offset"] == 0
    assert row["unit"] == "m"
    assert row["description"] == ""


def test_build_row_sheet2_style_different_column_names():
    row = build_row(
        {"Label": "Valve1", "Tag Address": "10005", "Data Type": "onoffboolean", "Unit": ""},
        SHEET2_COLUMNS,
        DATA_TYPE_MAP,
        DEFAULTS,
    )
    assert row["registerType"] == "DiscreteInput"
    assert row["address"] == 4
    assert row["format"] == ""


def test_build_row_missing_label_raises():
    with pytest.raises(RowError):
        build_row(
            {"Label": "", "Address": "40001", "Data Type": "float", "Unit": "m"},
            SHEET1_COLUMNS,
            DATA_TYPE_MAP,
            DEFAULTS,
        )


def test_build_row_missing_address_raises():
    with pytest.raises(RowError):
        build_row(
            {"Label": "Tank Level", "Address": None, "Data Type": "float", "Unit": "m"},
            SHEET1_COLUMNS,
            DATA_TYPE_MAP,
            DEFAULTS,
        )


def test_build_row_bad_data_type_raises():
    with pytest.raises(RowError):
        build_row(
            {"Label": "Tank Level", "Address": "40001", "Data Type": "mystery", "Unit": "m"},
            SHEET1_COLUMNS,
            DATA_TYPE_MAP,
            DEFAULTS,
        )


# --- convert_workbook / write_csv (full-file) -------------------------------

MAPPING = {
    "sheets": {
        "Sheet1": {"columns": {"Label": "label", "Address": "address", "Data Type": "format", "Unit": "unit"}},
        "Sheet2": {"columns": {"Label": "label", "Tag Address": "address", "Data Type": "format", "Unit": "unit"}},
    },
    "data_type_map": DATA_TYPE_MAP,
    "defaults": {"byteOrder": "ABCD", "scale": 1, "offset": 0},
}


def _build_test_workbook() -> openpyxl.Workbook:
    wb = openpyxl.Workbook()
    sheet1 = wb.active
    sheet1.title = "Sheet1"
    sheet1.append(["Address", "Parameter Id", "Label", "Data Type", "Unit"])
    sheet1.append([40001, "P1", "Tank Level", "float", "m"])
    sheet1.append([40011, "P2", "Line Pressure", "mystery_type", "kPa"])  # bad row, skipped

    sheet2 = wb.create_sheet("Sheet2")
    sheet2.append(["Label", "Data Type", "Unit", "Min", "Max", "Tag Address"])
    sheet2.append(["Valve1", "onoffboolean", "", 0, 1, "10005"])

    return wb


def test_convert_workbook_concatenates_sheets_and_skips_bad_rows():
    rows, warnings = convert_workbook(_build_test_workbook(), MAPPING)

    assert len(rows) == 2
    assert rows[0]["label"] == "Tank Level"
    assert rows[0]["registerType"] == "HoldingRegister"
    assert rows[0]["address"] == 0
    assert rows[1]["label"] == "Valve1"
    assert rows[1]["registerType"] == "DiscreteInput"
    assert rows[1]["address"] == 4

    assert len(warnings) == 1
    assert "Sheet1" in warnings[0]
    assert "row 3" in warnings[0]


def test_convert_workbook_raises_when_sheet_missing_mapped_column():
    bad_mapping = {
        "sheets": {"Sheet1": {"columns": {"Label": "label", "Nonexistent Column": "address"}}},
        "data_type_map": {},
        "defaults": {},
    }
    with pytest.raises(MappingError):
        convert_workbook(_build_test_workbook(), bad_mapping)


def test_write_csv_produces_expected_header_and_rows(tmp_path):
    rows, _ = convert_workbook(_build_test_workbook(), MAPPING)
    output_path = tmp_path / "out.csv"

    write_csv(rows, str(output_path))

    with open(output_path, newline="", encoding="utf-8") as f:
        reader = csv.reader(f)
        lines = list(reader)

    assert lines[0] == OUTPUT_COLUMNS
    assert lines[1][0] == "Tank Level"
    assert lines[1][2] == "HoldingRegister"
    assert lines[2][0] == "Valve1"
    assert lines[2][2] == "DiscreteInput"
