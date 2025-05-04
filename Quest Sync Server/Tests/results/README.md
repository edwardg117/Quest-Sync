# Test Results Directory

This directory contains the results of running the Quest Sync Server unit tests.

## File Format

Test results are saved in either XML or JSON format, depending on which script was used to run the tests:

- `run-tests.bat` - Saves results in XML format
- `run-tests-json.bat` - Saves results in JSON format

## Filename Format

Result files are named using the following format:

```
test-results-YYYY-MM-DD_HH-MM-SS.xml
```

or

```
test-results-YYYY-MM-DD_HH-MM-SS.json
```

Where:
- `YYYY-MM-DD` is the date the tests were run
- `HH-MM-SS` is the time the tests were run

## Viewing Results

XML results can be viewed in any web browser or XML viewer.
JSON results can be viewed in any text editor or JSON viewer.

## Automated Testing

These result files can be used for continuous integration or tracking test progress over time.
