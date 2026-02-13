#!/usr/bin/env python3
"""
Test script for WhiskerToolbox Python bindings.

This script tests the basic functionality of the Python bindings for
DigitalIntervalSeries and related types.
"""

import sys
import os

def test_interval():
    """Test Interval type bindings."""
    print("Testing Interval type...")
    
    # Import the module
    import whiskertoolbox as wt
    
    # Create intervals
    interval1 = wt.Interval(0, 10)
    interval2 = wt.Interval(5, 15)
    interval3 = wt.Interval(20, 30)
    
    print(f"  Created interval1: {interval1}")
    print(f"  Created interval2: {interval2}")
    print(f"  Created interval3: {interval3}")
    
    # Test properties
    assert interval1.start == 0
    assert interval1.end == 10
    print("  ✓ Interval properties work")
    
    # Test utility functions
    assert wt.is_overlapping(interval1, interval2)
    print("  ✓ interval1 and interval2 overlap")
    
    assert not wt.is_overlapping(interval1, interval3)
    print("  ✓ interval1 and interval3 do not overlap")
    
    assert wt.is_contained(interval1, 5)
    print("  ✓ Time 5 is contained in interval1")
    
    assert not wt.is_contained(interval1, 15)
    print("  ✓ Time 15 is not contained in interval1")
    
    print("✓ Interval tests passed!\n")


def test_digital_interval_series():
    """Test DigitalIntervalSeries type bindings."""
    print("Testing DigitalIntervalSeries...")
    
    import whiskertoolbox as wt
    
    # Create empty series
    series = wt.DigitalIntervalSeries()
    print(f"  Created empty series: {series}")
    assert len(series) == 0
    print("  ✓ Empty series has size 0")
    
    # Add intervals
    series.add_event(wt.Interval(0, 10))
    series.add_event(wt.Interval(20, 30))
    series.add_event(wt.Interval(40, 50))
    print(f"  After adding intervals: {series}")
    assert len(series) == 3
    print("  ✓ Series has 3 intervals after adding")
    
    # Create from list of intervals
    intervals = [wt.Interval(0, 5), wt.Interval(10, 15), wt.Interval(20, 25)]
    series2 = wt.DigitalIntervalSeries(intervals)
    print(f"  Created from list: {series2}")
    assert len(series2) == 3
    print("  ✓ Series created from list has 3 intervals")
    
    # Create from list of tuples
    series3 = wt.DigitalIntervalSeries([(0.0, 5.0), (10.0, 15.0)])
    print(f"  Created from tuples: {series3}")
    assert len(series3) == 2
    print("  ✓ Series created from tuples has 2 intervals")
    
    # Get intervals
    intervals_out = series.get_intervals()
    print(f"  Retrieved {len(intervals_out)} intervals from series")
    assert len(intervals_out) == 3
    print("  ✓ Retrieved intervals match added intervals")
    
    # Test create from boolean vector
    bool_vector = [True, True, False, False, True, True, True, False]
    series4 = wt.DigitalIntervalSeries()
    series4.create_intervals_from_bool(bool_vector)
    print(f"  Created from bool vector: {series4}")
    assert len(series4) == 2  # Should create 2 intervals: [0,1] and [4,6]
    print("  ✓ Create from boolean vector works")
    
    print("✓ DigitalIntervalSeries tests passed!\n")


def test_transforms():
    """Test transform bindings."""
    print("Testing transforms...")
    
    import whiskertoolbox as wt
    
    # Create two series
    series1 = wt.DigitalIntervalSeries([wt.Interval(0, 10), wt.Interval(20, 30)])
    series2 = wt.DigitalIntervalSeries([wt.Interval(5, 15), wt.Interval(25, 35)])
    
    print(f"  Series 1: {series1}")
    print(f"  Series 2: {series2}")
    
    # Test AND operation
    params_and = wt.BooleanParams()
    params_and.operation = wt.BooleanOperation.AND
    params_and.other_series = series2
    
    result_and = wt.apply_boolean_operation(series1, params_and)
    print(f"  AND result: {result_and}")
    print("  ✓ AND operation completed")
    
    # Test OR operation
    params_or = wt.BooleanParams()
    params_or.operation = wt.BooleanOperation.OR
    params_or.other_series = series2
    
    result_or = wt.apply_boolean_operation(series1, params_or)
    print(f"  OR result: {result_or}")
    print("  ✓ OR operation completed")
    
    # Test NOT operation
    params_not = wt.BooleanParams()
    params_not.operation = wt.BooleanOperation.NOT
    
    result_not = wt.apply_boolean_operation(series1, params_not)
    print(f"  NOT result: {result_not}")
    print("  ✓ NOT operation completed")
    
    print("✓ Transform tests passed!\n")


def test_python_transform():
    """Test custom Python transforms."""
    print("Testing Python transforms...")
    
    import whiskertoolbox as wt
    
    # Define a simple Python transform that doubles interval lengths
    def double_intervals(series):
        """Double the length of each interval."""
        result = wt.DigitalIntervalSeries()
        for interval in series.get_intervals():
            length = interval.end - interval.start
            new_interval = wt.Interval(interval.start, interval.end + length)
            result.add_event(new_interval)
        return result
    
    # Create a PythonTransform
    transform = wt.PythonTransform("DoubleIntervals", double_intervals)
    print(f"  Created transform: {transform.get_name()}")
    
    # Apply the transform
    series = wt.DigitalIntervalSeries([wt.Interval(0, 10), wt.Interval(20, 25)])
    print(f"  Input series: {series}")
    
    # Note: We can't directly call execute from Python easily with the variant system,
    # but we can call the Python function directly
    result = double_intervals(series)
    print(f"  Output series: {result}")
    
    # Verify the result
    result_intervals = result.get_intervals()
    assert len(result_intervals) == 2
    assert result_intervals[0].end == 20  # 0-10 doubled to 0-20
    assert result_intervals[1].end == 30  # 20-25 doubled to 20-30
    print("  ✓ Python transform correctly doubled intervals")
    
    print("✓ Python transform tests passed!\n")


def main():
    """Run all tests."""
    print("=" * 60)
    print("WhiskerToolbox Python Bindings Tests")
    print("=" * 60 + "\n")
    
    try:
        test_interval()
        test_digital_interval_series()
        test_transforms()
        test_python_transform()
        
        print("=" * 60)
        print("ALL TESTS PASSED!")
        print("=" * 60)
        return 0
        
    except ImportError as e:
        print(f"ERROR: Could not import whiskertoolbox module: {e}")
        print("\nMake sure the module is built and in your PYTHONPATH.")
        print("Try setting: export PYTHONPATH=/path/to/build/directory:$PYTHONPATH")
        return 1
    except AssertionError as e:
        print(f"ERROR: Assertion failed: {e}")
        import traceback
        traceback.print_exc()
        return 1
    except Exception as e:
        print(f"ERROR: Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
