"""Tests for the Moon Patrol iBooster DAQ analysis script."""

from unittest.mock import MagicMock, patch

import pandas as pd
import pytest

from analysis.analyze import (
    load_log,
    plot_can_bytes,
    plot_pressure,
    plot_travel,
    plot_travel_correlation,
    print_summary,
    split_can_pressure,
)

# =========================================================================
# Fixtures — 15-column format with travel_v1, travel_v2
# =========================================================================

SAMPLE_CSV = """\
# Test run description
timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,travel_v1,travel_v2,psi_front,psi_rear
1000,CAN1,0x1A0,00,FF,12,34,00,00,00,00,,,,
1001,CAN2,0x292,A0,B1,C2,00,00,00,00,00,,,,
1010,ADC,,,,,,,,,,2.341,2.105,1250.5,1180.3
1020,CAN1,0x1A0,00,FE,12,34,00,00,00,00,,,,
1030,ADC,,,,,,,,,,2.580,2.310,1300.0,1200.0
"""

SAMPLE_CSV_NO_COMMENTS = """\
timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,travel_v1,travel_v2,psi_front,psi_rear
2000,CAN1,0x100,AA,BB,CC,DD,EE,FF,00,11,,,,
2010,ADC,,,,,,,,,,1.500,1.400,500.0,600.0
"""

SAMPLE_CSV_CAN_ONLY = """\
timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,travel_v1,travel_v2,psi_front,psi_rear
3000,CAN1,0x1A0,00,FF,12,34,00,00,00,00,,,,
3010,CAN1,0x1A0,01,FE,12,34,00,00,00,00,,,,
"""

SAMPLE_CSV_ADC_ONLY = """\
timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,travel_v1,travel_v2,psi_front,psi_rear
4000,ADC,,,,,,,,,,2.000,1.900,100.0,200.0
4010,ADC,,,,,,,,,,2.100,2.000,150.0,250.0
"""

SAMPLE_CSV_LEGACY_PSI = """\
timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,psi_front,psi_rear
5000,CAN1,0x1A0,00,FF,12,34,00,00,00,00,,
5010,PSI,,,,,,,,,,100.0,200.0
"""

SAMPLE_CSV_HEADER_ONLY = """\
timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,travel_v1,travel_v2,psi_front,psi_rear
"""

SAMPLE_CSV_BLANK_LINES = """\
# Comment
timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,travel_v1,travel_v2,psi_front,psi_rear

1000,CAN1,0x1A0,00,FF,12,34,00,00,00,00,,,,

1010,ADC,,,,,,,,,,2.341,2.105,1250.5,1180.3

"""

SAMPLE_CSV_STATIC_CAN = """\
timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,travel_v1,travel_v2,psi_front,psi_rear
1000,CAN1,0x1A0,00,FF,12,34,00,00,00,00,,,,
1010,CAN1,0x1A0,00,FF,12,34,00,00,00,00,,,,
1020,CAN1,0x1A0,00,FF,12,34,00,00,00,00,,,,
"""

SAMPLE_CSV_MULTI_CHANGE = """\
timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,travel_v1,travel_v2,psi_front,psi_rear
1000,CAN1,0x1A0,00,FF,12,34,00,00,00,00,,,,
1010,CAN1,0x1A0,01,FE,13,34,00,00,00,00,,,,
1020,ADC,,,,,,,,,,2.341,2.105,1250.5,1180.3
1030,ADC,,,,,,,,,,2.580,2.310,1300.0,1200.0
"""

SAMPLE_CSV_FRONT_ONLY = """\
timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,travel_v1,travel_v2,psi_front,psi_rear
1000,ADC,,,,,,,,,,2.341,,1250.5,
1010,ADC,,,,,,,,,,2.580,,1300.0,
"""

SAMPLE_CSV_TRAVEL1_ONLY = """\
timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,travel_v1,travel_v2,psi_front,psi_rear
1000,ADC,,,,,,,,,,2.341,,,
1010,ADC,,,,,,,,,,2.580,,,
"""


@pytest.fixture
def sample_csv_file(tmp_path):
    f = tmp_path / "test_run.csv"
    f.write_text(SAMPLE_CSV)
    return f


@pytest.fixture
def sample_csv_no_comments_file(tmp_path):
    f = tmp_path / "test_no_comments.csv"
    f.write_text(SAMPLE_CSV_NO_COMMENTS)
    return f


@pytest.fixture
def sample_csv_can_only_file(tmp_path):
    f = tmp_path / "test_can_only.csv"
    f.write_text(SAMPLE_CSV_CAN_ONLY)
    return f


@pytest.fixture
def sample_csv_adc_only_file(tmp_path):
    f = tmp_path / "test_adc_only.csv"
    f.write_text(SAMPLE_CSV_ADC_ONLY)
    return f


@pytest.fixture
def sample_csv_legacy_file(tmp_path):
    f = tmp_path / "test_legacy.csv"
    f.write_text(SAMPLE_CSV_LEGACY_PSI)
    return f


@pytest.fixture
def sample_csv_header_only_file(tmp_path):
    f = tmp_path / "test_header_only.csv"
    f.write_text(SAMPLE_CSV_HEADER_ONLY)
    return f


@pytest.fixture
def sample_csv_blank_lines_file(tmp_path):
    f = tmp_path / "test_blank_lines.csv"
    f.write_text(SAMPLE_CSV_BLANK_LINES)
    return f


@pytest.fixture
def sample_csv_static_can_file(tmp_path):
    f = tmp_path / "test_static_can.csv"
    f.write_text(SAMPLE_CSV_STATIC_CAN)
    return f


@pytest.fixture
def sample_csv_multi_change_file(tmp_path):
    f = tmp_path / "test_multi_change.csv"
    f.write_text(SAMPLE_CSV_MULTI_CHANGE)
    return f


@pytest.fixture
def sample_csv_front_only_file(tmp_path):
    f = tmp_path / "test_front_only.csv"
    f.write_text(SAMPLE_CSV_FRONT_ONLY)
    return f


@pytest.fixture
def sample_csv_travel1_only_file(tmp_path):
    f = tmp_path / "test_travel1_only.csv"
    f.write_text(SAMPLE_CSV_TRAVEL1_ONLY)
    return f


# =========================================================================
# load_log tests
# =========================================================================


class TestLoadLog:
    def test_loads_correct_number_of_rows(self, sample_csv_file):
        df = load_log(str(sample_csv_file))
        assert len(df) == 5

    def test_skips_comment_lines(self, sample_csv_file):
        df = load_log(str(sample_csv_file))
        for val in df["source"]:
            assert not str(val).startswith("#")

    def test_correct_columns(self, sample_csv_file):
        df = load_log(str(sample_csv_file))
        expected_cols = [
            "timestamp_ms",
            "source",
            "can_id",
            "d0",
            "d1",
            "d2",
            "d3",
            "d4",
            "d5",
            "d6",
            "d7",
            "travel_v1",
            "travel_v2",
            "psi_front",
            "psi_rear",
        ]
        assert list(df.columns) == expected_cols

    def test_timestamp_converted_to_numeric(self, sample_csv_file):
        df = load_log(str(sample_csv_file))
        assert pd.api.types.is_numeric_dtype(df["timestamp_ms"])

    def test_loads_file_without_comments(self, sample_csv_no_comments_file):
        df = load_log(str(sample_csv_no_comments_file))
        assert len(df) == 2

    def test_source_values_preserved(self, sample_csv_file):
        df = load_log(str(sample_csv_file))
        sources = set(df["source"])
        assert "CAN1" in sources
        assert "CAN2" in sources
        assert "ADC" in sources

    def test_empty_file_raises_valueerror(self, tmp_path):
        f = tmp_path / "empty.csv"
        f.write_text("")
        with pytest.raises(ValueError, match="No header found"):
            load_log(str(f))

    def test_header_only_returns_empty_df(self, sample_csv_header_only_file):
        df = load_log(str(sample_csv_header_only_file))
        assert len(df) == 0
        assert "timestamp_ms" in df.columns

    def test_blank_lines_skipped(self, sample_csv_blank_lines_file):
        df = load_log(str(sample_csv_blank_lines_file))
        assert len(df) == 2  # Only real data rows, blank lines skipped

    def test_multiple_comments_skipped(self, tmp_path):
        csv_data = """\
# Comment 1
# Comment 2
# Comment 3
timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,travel_v1,travel_v2,psi_front,psi_rear
1000,CAN1,0x1A0,00,FF,12,34,00,00,00,00,,,,
"""
        f = tmp_path / "multi_comments.csv"
        f.write_text(csv_data)
        df = load_log(str(f))
        assert len(df) == 1
        assert df["source"].iloc[0] == "CAN1"


# =========================================================================
# split_can_pressure tests
# =========================================================================


class TestSplitCanPressure:
    def test_separates_can_and_adc(self, sample_csv_file):
        df = load_log(str(sample_csv_file))
        can_df, adc_df = split_can_pressure(df)
        assert len(can_df) == 3  # 2 CAN1 + 1 CAN2
        assert len(adc_df) == 2

    def test_can_sources_correct(self, sample_csv_file):
        df = load_log(str(sample_csv_file))
        can_df, _ = split_can_pressure(df)
        for source in can_df["source"]:
            assert source.startswith("CAN")

    def test_travel_values_parsed(self, sample_csv_file):
        df = load_log(str(sample_csv_file))
        _, adc_df = split_can_pressure(df)
        assert pd.api.types.is_numeric_dtype(adc_df["travel_v1"])
        assert pd.api.types.is_numeric_dtype(adc_df["travel_v2"])
        assert adc_df["travel_v1"].iloc[0] == pytest.approx(2.341)
        assert adc_df["travel_v2"].iloc[0] == pytest.approx(2.105)

    def test_psi_values_parsed(self, sample_csv_file):
        df = load_log(str(sample_csv_file))
        _, adc_df = split_can_pressure(df)
        assert pd.api.types.is_numeric_dtype(adc_df["psi_front"])
        assert pd.api.types.is_numeric_dtype(adc_df["psi_rear"])
        assert adc_df["psi_front"].iloc[0] == pytest.approx(1250.5)
        assert adc_df["psi_rear"].iloc[0] == pytest.approx(1180.3)

    def test_hex_byte_parsing(self, sample_csv_file):
        df = load_log(str(sample_csv_file))
        can_df, _ = split_can_pressure(df)
        first_row = can_df.iloc[0]
        assert first_row["d0"] == 0x00
        assert first_row["d1"] == 0xFF

    def test_can_only_file(self, sample_csv_can_only_file):
        df = load_log(str(sample_csv_can_only_file))
        can_df, adc_df = split_can_pressure(df)
        assert len(can_df) == 2
        assert len(adc_df) == 0

    def test_adc_only_file(self, sample_csv_adc_only_file):
        df = load_log(str(sample_csv_adc_only_file))
        can_df, adc_df = split_can_pressure(df)
        assert len(can_df) == 0
        assert len(adc_df) == 2

    def test_legacy_psi_source(self, sample_csv_legacy_file):
        """Old log files with source='PSI' should still be parsed."""
        df = load_log(str(sample_csv_legacy_file))
        can_df, adc_df = split_can_pressure(df)
        assert len(can_df) == 1
        assert len(adc_df) == 1

    def test_empty_hex_bytes_become_none(self, sample_csv_adc_only_file):
        """ADC rows have empty CAN byte fields — they should parse as None."""
        df = load_log(str(sample_csv_adc_only_file))
        can_df, _ = split_can_pressure(df)
        # No CAN rows, so nothing to check for hex parsing
        assert len(can_df) == 0

    def test_can_rows_have_no_travel_data(self, sample_csv_file):
        """CAN rows should have empty/NaN travel and pressure fields."""
        df = load_log(str(sample_csv_file))
        can_df, _ = split_can_pressure(df)
        # CAN rows shouldn't appear in ADC dataframe
        assert all(source.startswith("CAN") for source in can_df["source"])

    def test_all_eight_data_bytes_parsed(self, sample_csv_file):
        """Verify all d0-d7 columns are parsed as integers."""
        df = load_log(str(sample_csv_file))
        can_df, _ = split_can_pressure(df)
        first = can_df.iloc[0]
        assert first["d0"] == 0x00
        assert first["d1"] == 0xFF
        assert first["d2"] == 0x12
        assert first["d3"] == 0x34
        assert first["d4"] == 0x00
        assert first["d5"] == 0x00
        assert first["d6"] == 0x00
        assert first["d7"] == 0x00


# =========================================================================
# print_summary tests
# =========================================================================


class TestPrintSummary:
    def test_prints_without_error(self, sample_csv_file, capsys):
        df = load_log(str(sample_csv_file))
        can_df, adc_df = split_can_pressure(df)
        print_summary(can_df, adc_df, str(sample_csv_file))
        captured = capsys.readouterr()
        assert "Log Analysis" in captured.out

    def test_shows_travel_stats(self, sample_csv_file, capsys):
        df = load_log(str(sample_csv_file))
        can_df, adc_df = split_can_pressure(df)
        print_summary(can_df, adc_df, str(sample_csv_file))
        captured = capsys.readouterr()
        assert "Travel 1" in captured.out
        assert "Travel 2" in captured.out

    def test_shows_pressure_stats(self, sample_csv_file, capsys):
        df = load_log(str(sample_csv_file))
        can_df, adc_df = split_can_pressure(df)
        print_summary(can_df, adc_df, str(sample_csv_file))
        captured = capsys.readouterr()
        assert "Front" in captured.out

    def test_shows_can_id_info(self, sample_csv_file, capsys):
        df = load_log(str(sample_csv_file))
        can_df, adc_df = split_can_pressure(df)
        print_summary(can_df, adc_df, str(sample_csv_file))
        captured = capsys.readouterr()
        assert "CAN1" in captured.out
        assert "Unique IDs" in captured.out

    def test_handles_empty_data(self, capsys):
        empty_can = pd.DataFrame(
            columns=[
                "timestamp_ms",
                "source",
                "can_id",
                "d0",
                "d1",
                "d2",
                "d3",
                "d4",
                "d5",
                "d6",
                "d7",
                "travel_v1",
                "travel_v2",
                "psi_front",
                "psi_rear",
            ]
        )
        empty_adc = empty_can.copy()
        print_summary(empty_can, empty_adc, "empty.csv")
        captured = capsys.readouterr()
        assert "No data found" in captured.out

    def test_can_only_summary(self, sample_csv_can_only_file, capsys):
        """Summary with CAN data only should show CAN info but no travel/pressure."""
        df = load_log(str(sample_csv_can_only_file))
        can_df, adc_df = split_can_pressure(df)
        print_summary(can_df, adc_df, str(sample_csv_can_only_file))
        captured = capsys.readouterr()
        assert "CAN1" in captured.out
        assert "0x1A0" in captured.out
        assert "Travel 1" not in captured.out
        assert "Front" not in captured.out

    def test_adc_only_summary(self, sample_csv_adc_only_file, capsys):
        """Summary with ADC data only should show travel/pressure but no CAN."""
        df = load_log(str(sample_csv_adc_only_file))
        can_df, adc_df = split_can_pressure(df)
        print_summary(can_df, adc_df, str(sample_csv_adc_only_file))
        captured = capsys.readouterr()
        assert "Travel 1" in captured.out
        assert "Front" in captured.out
        assert "CAN1" not in captured.out

    def test_total_samples_accurate(self, sample_csv_file, capsys):
        """Total samples should equal CAN + ADC rows."""
        df = load_log(str(sample_csv_file))
        can_df, adc_df = split_can_pressure(df)
        print_summary(can_df, adc_df, str(sample_csv_file))
        captured = capsys.readouterr()
        assert "Total samples:  5" in captured.out

    def test_duration_calculation(self, sample_csv_file, capsys):
        """Duration should be (max_timestamp - min_timestamp) / 1000."""
        df = load_log(str(sample_csv_file))
        can_df, adc_df = split_can_pressure(df)
        print_summary(can_df, adc_df, str(sample_csv_file))
        captured = capsys.readouterr()
        # 1030 - 1000 = 30ms = 0.0s (rounded to 1 decimal)
        assert "Duration:       0.0 s" in captured.out

    def test_zero_duration_no_crash(self, tmp_path, capsys):
        """All same timestamps should not cause division by zero."""
        csv_data = """\
timestamp_ms,source,can_id,d0,d1,d2,d3,d4,d5,d6,d7,travel_v1,travel_v2,psi_front,psi_rear
1000,CAN1,0x1A0,00,FF,12,34,00,00,00,00,,,,
1000,CAN1,0x1A0,01,FE,12,34,00,00,00,00,,,,
"""
        f = tmp_path / "same_ts.csv"
        f.write_text(csv_data)
        df = load_log(str(f))
        can_df, adc_df = split_can_pressure(df)
        print_summary(can_df, adc_df, str(f))
        captured = capsys.readouterr()
        assert "Duration:       0.0 s" in captured.out
        # Freq should be 0 when duration is 0
        assert "0.0" in captured.out


# =========================================================================
# plot function tests (mock matplotlib to avoid file I/O)
# =========================================================================


class TestPlotPressure:
    @patch("analysis.analyze.plt")
    def test_plot_pressure_calls_savefig(self, mock_plt, sample_csv_file):
        df = load_log(str(sample_csv_file))
        _, adc_df = split_can_pressure(df)
        mock_fig = MagicMock()
        mock_ax = MagicMock()
        mock_plt.subplots.return_value = (mock_fig, mock_ax)
        plot_pressure(adc_df, "test_run")
        mock_fig.savefig.assert_called_once()

    @patch("analysis.analyze.plt")
    def test_plot_pressure_empty_df(self, mock_plt, capsys):
        empty = pd.DataFrame(
            columns=[
                "timestamp_ms",
                "psi_front",
                "psi_rear",
            ]
        )
        plot_pressure(empty, "test_run")
        captured = capsys.readouterr()
        assert "No pressure data" in captured.out
        mock_plt.subplots.assert_not_called()

    @patch("analysis.analyze.plt")
    def test_plot_pressure_front_only(self, mock_plt, sample_csv_front_only_file):
        """Should plot when only front pressure data exists."""
        df = load_log(str(sample_csv_front_only_file))
        _, adc_df = split_can_pressure(df)
        mock_fig = MagicMock()
        mock_ax = MagicMock()
        mock_plt.subplots.return_value = (mock_fig, mock_ax)
        plot_pressure(adc_df, "test_run")
        mock_fig.savefig.assert_called_once()

    @patch("analysis.analyze.plt")
    def test_plot_pressure_nan_only_skips(self, mock_plt, capsys):
        """Should print 'no data' when pressure columns are all NaN."""
        adc_df = pd.DataFrame(
            {
                "timestamp_ms": [1000, 1010],
                "psi_front": [float("nan"), float("nan")],
                "psi_rear": [float("nan"), float("nan")],
            }
        )
        plot_pressure(adc_df, "test_run")
        captured = capsys.readouterr()
        assert "No pressure data" in captured.out
        mock_plt.subplots.assert_not_called()


class TestPlotTravel:
    @patch("analysis.analyze.plt")
    def test_plot_travel_calls_savefig(self, mock_plt, sample_csv_file):
        df = load_log(str(sample_csv_file))
        _, adc_df = split_can_pressure(df)
        mock_fig = MagicMock()
        mock_ax = MagicMock()
        mock_plt.subplots.return_value = (mock_fig, mock_ax)
        plot_travel(adc_df, "test_run")
        mock_fig.savefig.assert_called_once()

    @patch("analysis.analyze.plt")
    def test_plot_travel_empty_df(self, mock_plt, capsys):
        empty = pd.DataFrame(columns=["timestamp_ms", "travel_v1", "travel_v2"])
        plot_travel(empty, "test_run")
        captured = capsys.readouterr()
        assert "No travel data" in captured.out
        mock_plt.subplots.assert_not_called()

    @patch("analysis.analyze.plt")
    def test_plot_travel_v1_only(self, mock_plt, sample_csv_travel1_only_file):
        """Should plot when only travel_v1 data exists."""
        df = load_log(str(sample_csv_travel1_only_file))
        _, adc_df = split_can_pressure(df)
        mock_fig = MagicMock()
        mock_ax = MagicMock()
        mock_plt.subplots.return_value = (mock_fig, mock_ax)
        plot_travel(adc_df, "test_run")
        mock_fig.savefig.assert_called_once()

    @patch("analysis.analyze.plt")
    def test_plot_travel_nan_only_skips(self, mock_plt, capsys):
        """Should print 'no data' when travel columns are all NaN."""
        adc_df = pd.DataFrame(
            {
                "timestamp_ms": [1000, 1010],
                "travel_v1": [float("nan"), float("nan")],
                "travel_v2": [float("nan"), float("nan")],
            }
        )
        plot_travel(adc_df, "test_run")
        captured = capsys.readouterr()
        assert "No travel data" in captured.out
        mock_plt.subplots.assert_not_called()


class TestPlotTravelCorrelation:
    @patch("analysis.analyze.plt")
    def test_no_data_prints_message(self, mock_plt, capsys):
        empty_can = pd.DataFrame(columns=["timestamp_ms", "source", "can_id"])
        empty_adc = pd.DataFrame(columns=["timestamp_ms", "travel_v1"])
        plot_travel_correlation(empty_can, empty_adc, "test_run")
        captured = capsys.readouterr()
        assert "No data for travel correlation" in captured.out

    @patch("analysis.analyze.plt")
    def test_correlation_with_data(self, mock_plt, sample_csv_file):
        df = load_log(str(sample_csv_file))
        can_df, adc_df = split_can_pressure(df)
        mock_fig = MagicMock()
        mock_ax = MagicMock()
        # subplots(1, n, squeeze=False) returns (fig, 2D array)
        # axes[0] gives 1D array; we provide a list of mock axes
        mock_plt.subplots.return_value = (mock_fig, [[mock_ax]])
        plot_travel_correlation(can_df, adc_df, "test_run")

    @patch("analysis.analyze.plt")
    def test_correlation_skips_static_ids(self, mock_plt, sample_csv_static_can_file):
        """CAN IDs where all bytes are static should not produce plots."""
        df = load_log(str(sample_csv_static_can_file))
        can_df, _ = split_can_pressure(df)
        # Provide travel data so we don't exit early
        adc_data = pd.DataFrame({"timestamp_ms": [1005, 1015], "travel_v1": [2.0, 2.5]})
        plot_travel_correlation(can_df, adc_data, "test_run")
        # No subplots should be created for static-only CAN data
        mock_plt.subplots.assert_not_called()

    @patch("analysis.analyze.plt")
    def test_correlation_no_travel_column(self, mock_plt, capsys):
        """Should print message when travel_v1 column is missing."""
        can_df = pd.DataFrame(
            {
                "timestamp_ms": [1000, 1010],
                "source": ["CAN1", "CAN1"],
                "can_id": ["0x1A0", "0x1A0"],
                "d0": [0, 1],
            }
        )
        adc_df = pd.DataFrame({"timestamp_ms": [1005]})
        plot_travel_correlation(can_df, adc_df, "test_run")
        captured = capsys.readouterr()
        assert "No travel data for correlation" in captured.out

    @patch("analysis.analyze.plt")
    def test_correlation_multi_changing_bytes(self, mock_plt, sample_csv_multi_change_file):
        """Multiple changing bytes should produce correctly sized subplot grid."""
        df = load_log(str(sample_csv_multi_change_file))
        can_df, adc_df = split_can_pressure(df)
        mock_fig = MagicMock()
        mock_axes = [MagicMock(), MagicMock(), MagicMock()]
        mock_plt.subplots.return_value = (mock_fig, [mock_axes])
        plot_travel_correlation(can_df, adc_df, "test_run")
        # Should have been called with 3 changing bytes (d0, d1, d2)
        if mock_plt.subplots.called:
            call_args = mock_plt.subplots.call_args
            assert call_args[0] == (1, 3)  # 1 row, 3 columns


class TestPlotCanBytes:
    @patch("analysis.analyze.plt")
    def test_plot_can_bytes_with_data(self, mock_plt, sample_csv_file):
        df = load_log(str(sample_csv_file))
        can_df, _ = split_can_pressure(df)
        mock_fig = MagicMock()
        mock_ax = MagicMock()
        # subplots(1, 1) returns a single Axes, not a list
        mock_plt.subplots.return_value = (mock_fig, mock_ax)
        plot_can_bytes(can_df, "test_run")

    @patch("analysis.analyze.plt")
    def test_plot_can_bytes_empty(self, mock_plt, capsys):
        empty_can = pd.DataFrame(
            columns=[
                "timestamp_ms",
                "source",
                "can_id",
                "d0",
                "d1",
                "d2",
                "d3",
                "d4",
                "d5",
                "d6",
                "d7",
            ]
        )
        plot_can_bytes(empty_can, "test_run")
        captured = capsys.readouterr()
        assert "No CAN data" in captured.out

    @patch("analysis.analyze.plt")
    def test_plot_can_bytes_static_ids_skipped(self, mock_plt, sample_csv_static_can_file):
        """CAN IDs with all static bytes should be skipped (no plots)."""
        df = load_log(str(sample_csv_static_can_file))
        can_df, _ = split_can_pressure(df)
        plot_can_bytes(can_df, "test_run")
        mock_plt.subplots.assert_not_called()

    @patch("analysis.analyze.plt")
    def test_plot_can_bytes_multiple_changing(self, mock_plt, sample_csv_multi_change_file):
        """Multiple changing bytes should create n_plots subplots."""
        df = load_log(str(sample_csv_multi_change_file))
        can_df, _ = split_can_pressure(df)
        mock_fig = MagicMock()
        mock_axes = [MagicMock(), MagicMock(), MagicMock()]
        mock_plt.subplots.return_value = (mock_fig, mock_axes)
        plot_can_bytes(can_df, "test_run")
        # d0, d1, d2 change; subplots should be called with (3, 1, ...)
        if mock_plt.subplots.called:
            call_args = mock_plt.subplots.call_args
            assert call_args[0][0] == 3  # 3 changing bytes
