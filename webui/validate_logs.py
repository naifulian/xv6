#!/usr/bin/env python3
"""Validate xv6 perftest logs before plotting or publishing them."""

import math

from experiment_data import SCENARIOS, load_branch_dataset, required_tests_for, result_is_valid, result_signature


def is_valid(result):
    return result_is_valid(result)


def low_resolution_runs(result):
    flagged = []
    if not result:
        return flagged

    for row in result.get('per_run', []):
        if row.get('status') != 'complete':
            continue
        if row.get('unit') != 'ticks':
            continue
        if row.get('avg', 0) > 3 or row.get('std', 0) != 0:
            continue
        if row.get('runs', 0) < 5:
            continue
        flagged.append(row)

    return flagged


def low_resolution(result):
    return bool(low_resolution_runs(result))


def suggested_batch(result, flagged_runs=None):
    runs = flagged_runs if flagged_runs is not None else low_resolution_runs(result)
    if runs:
        avg = max(min(row.get('avg', 0) for row in runs), 1)
        batch = max(max(row.get('batch', 1) for row in runs), 1)
    else:
        avg = max(result.get('avg', 0) or 0, 1)
        batch = max(result.get('batch', 1) or 1, 1)
    factor = max(2, int(math.ceil(8.0 / avg)))
    return batch * factor


def uses_split_boot(dataset):
    commands = dataset['collection_summary'].get('unique_commands', [])
    return any('perftest memory + perftest sched' in command for command in commands)


def validate_metadata(dataset, branch_name):
    errors = []
    warnings = []
    metadata = dataset['metadata']
    summary = dataset['collection_summary']

    if not dataset['runs']:
        return [f'[{branch_name}] No experiment logs found'], warnings

    if not metadata:
        warnings.append(f'[{branch_name}] No META data found in the discovered logs')
    else:
        if metadata.get('branch') != branch_name:
            errors.append(f'[{branch_name}] Branch mismatch: expected {branch_name}, got {metadata.get("branch")}')
        if metadata.get('cpus', 0) < 1:
            errors.append(f'[{branch_name}] Invalid CPU count: {metadata.get("cpus")}')

    if dataset['manifest'] and len(dataset['manifest']) != len(dataset['runs']):
        warnings.append(f'[{branch_name}] Manifest rows ({len(dataset["manifest"])}) do not match discovered runs ({len(dataset["runs"])})')
    if summary.get('partial_runs', 0) > 0:
        warnings.append(f'[{branch_name}] {summary["partial_runs"]} partial run(s) detected; keep them for audit trail but do not count them as thesis rounds')
    if summary.get('raw_only_runs', 0) > 0:
        warnings.append(f'[{branch_name}] {summary["raw_only_runs"]} run(s) were recovered from raw logs; rerun with --resume if you need complete rounds')
    if len(summary.get('unique_commits', [])) > 1:
        warnings.append(f'[{branch_name}] Multiple commits detected in manifest: {", ".join(summary["unique_commits"])}')
    if len(summary.get('unique_commands', [])) > 1:
        warnings.append(f'[{branch_name}] Multiple perftest commands detected in manifest; avoid mixing workloads in one formal dataset')
    if branch_name == 'testing' and summary.get('unique_commands') and not uses_split_boot(dataset):
        warnings.append(f'[{branch_name}] Formal scheduler experiments should use split boots (`perftest memory` + `perftest sched`) to reduce cross-workload contamination')
    if len(summary.get('unique_cpus', [])) > 1:
        warnings.append(f'[{branch_name}] Multiple CPU settings detected in manifest: {", ".join(summary["unique_cpus"])}')

    for row in dataset.get('manifest', []):
        run_id = row.get('run_id', 'unknown')
        status = row.get('status', '')
        exit_code = int(row.get('exit_code') or 0)
        commands_planned = int(row.get('commands_planned') or 0)
        commands_completed = int(row.get('commands_completed') or 0)
        failed_command = row.get('failed_command', '')

        if status == 'partial' and failed_command and exit_code == 0:
            errors.append(f'[{branch_name}] Manifest row {run_id} records failed command `{failed_command}` but exit_code=0; recollect or rescan before trusting this dataset')
        if status == 'complete' and exit_code != 0:
            errors.append(f'[{branch_name}] Manifest row {run_id} is marked complete but exit_code={exit_code}')
        if status == 'complete' and commands_planned and commands_completed < commands_planned:
            errors.append(f'[{branch_name}] Manifest row {run_id} is marked complete but only recorded {commands_completed}/{commands_planned} finished commands')

    return errors, warnings


def validate_results(dataset, branch_name):
    errors = []
    warnings = []
    results = dataset['results']
    required = required_tests_for(branch_name)
    total_rounds = len(dataset['runs'])

    for test_name in required:
        result = results.get(test_name)
        if not result:
            errors.append(f'[{branch_name}] Missing required test: {test_name}')
            continue
        if result.get('consistency_error'):
            errors.append(f'[{branch_name}] {test_name} cannot be aggregated safely: {result["consistency_error"]}')
            continue
        if not is_valid(result):
            errors.append(f'[{branch_name}] Required test {test_name} has no valid measurement')
            continue
        if result.get('rounds_present', 0) < total_rounds:
            warnings.append(f'[{branch_name}] {test_name} only appeared in {result.get("rounds_present", 0)}/{total_rounds} discovered rounds')
        if result.get('round_count', 0) == 1 and total_rounds >= 2:
            warnings.append(f'[{branch_name}] {test_name} has only one valid round; repeatability is weak')
        low_res_runs = low_resolution_runs(result)
        if low_res_runs:
            run_ids = ', '.join(row.get('run_id', 'unknown') for row in low_res_runs)
            warnings.append(
                f'[{branch_name}] {test_name} is low-resolution in complete run(s) {run_ids}; try batch>={suggested_batch(result, low_res_runs)}'
            )
        round_cv = result.get('round_cv_percent')
        if round_cv is not None and round_cv >= 10.0:
            warnings.append(f'[{branch_name}] {test_name} has high cross-round variation ({round_cv:.2f}% CV)')

    if dataset['collection_summary'].get('complete_runs', 0) < 3:
        warnings.append(
            f'[{branch_name}] Only {dataset["collection_summary"].get("complete_runs", 0)} complete round(s) detected; thesis-quality experiments should use at least 3 complete rounds'
        )

    return errors, warnings


def validate_consistency(baseline_data, testing_data):
    errors = []
    warnings = []

    for test_name in required_tests_for('baseline'):
        if test_name not in baseline_data['results']:
            errors.append(f'[baseline] Missing common test: {test_name}')
            continue
        if test_name not in testing_data['results']:
            errors.append(f'[testing] Missing common test: {test_name}')
            continue

        base = baseline_data['results'].get(test_name)
        test = testing_data['results'].get(test_name)
        if is_valid(base) and is_valid(test) and result_signature(base) != result_signature(test):
            warnings.append(
                f'[cross-branch] {test_name} uses different signatures: baseline {result_signature(base)}, testing {result_signature(test)}; recollect matching batches before writing direct comparison conclusions'
            )

    return errors, warnings


def validate_scheduler_resolution(testing_data):
    warnings = []
    results = testing_data['results']

    for scenario, tests in SCENARIOS.items():
        values = [results[test]['avg'] for test in tests if test in results and is_valid(results[test])]
        if len(values) >= 2 and max(values) > 0:
            spread = (max(values) - min(values)) * 100.0 / max(values)
            if spread < 5.0:
                warnings.append(f'[testing] Scheduler scenario {scenario} has weak discrimination ({spread:.2f}% spread)')
        if len(values) == len(tests) and len(set(values)) == 1:
            warnings.append(f'[testing] Scheduler scenario {scenario} has identical results; the workload may not distinguish algorithms')

    return warnings


def recommend_actions(baseline_data, testing_data):
    actions = []

    for branch_name, dataset in (('baseline', baseline_data), ('testing', testing_data)):
        summary = dataset['collection_summary']
        complete_runs = summary.get('complete_runs', 0)
        if complete_runs < 3:
            actions.append(
                f'[{branch_name}] Add at least {3 - complete_runs} more complete round(s) with `./webui/analyze_all.sh collect --rounds 3 --resume` before writing formal conclusions.'
            )

        partial_ids = [run['run_id'] for run in dataset.get('run_statuses', []) if run.get('status') == 'partial']
        if partial_ids:
            actions.append(
                f'[{branch_name}] Partial runs detected ({", ".join(partial_ids)}); keep them as interruption evidence, but do not count them as formal rounds.'
            )

        raw_ids = [run['run_id'] for run in dataset.get('run_statuses', []) if run.get('source_kind') == 'raw']
        if raw_ids:
            actions.append(
                f'[{branch_name}] Raw-only runs were recovered ({", ".join(raw_ids)}); rerun with `--resume` if you want clean, complete rounds for the thesis dataset.'
            )
        if branch_name == 'testing' and summary.get('unique_commands') and not uses_split_boot(dataset):
            actions.append(
                '[testing] Recollect formal data with split boots (`perftest memory` + `perftest sched`) so scheduler conclusions are not contaminated by the earlier memory workload.'
            )

        if dataset.get('aggregation_errors'):
            actions.append(
                f'[{branch_name}] Recollect or separate datasets before aggregation; {len(dataset["aggregation_errors"])} test(s) currently mix different unit/batch/runs signatures.'
            )

        for test_name, result in sorted(dataset['results'].items()):
            low_res_runs = low_resolution_runs(result)
            if low_res_runs:
                actions.append(
                    f'[{branch_name}] Increase `{test_name}` batch from {max(row.get("batch", 1) for row in low_res_runs)} to at least {suggested_batch(result, low_res_runs)} to move it out of the 1-3 tick range.'
                )

    for scenario, tests in SCENARIOS.items():
        values = [testing_data['results'][test]['avg'] for test in tests if test in testing_data['results'] and is_valid(testing_data['results'][test])]
        if len(values) >= 2 and max(values) > 0:
            spread = (max(values) - min(values)) * 100.0 / max(values)
            if spread < 5.0:
                actions.append(
                    f'[testing] Rework scheduler scenario `{scenario}`; current spread is only {spread:.2f}%, which is too weak for a convincing thesis comparison.'
                )

    deduped = []
    seen = set()
    for item in actions:
        if item not in seen:
            deduped.append(item)
            seen.add(item)
    return deduped


def print_summary(baseline_data, testing_data):
    print()
    print('=' * 68)
    print('LOG FILE SUMMARY')
    print('=' * 68)

    for title, data in (('Baseline', baseline_data), ('Testing', testing_data)):
        summary = data['collection_summary']
        print(f'\n[{title}]')
        print(f'  Runs discovered: {len(data["runs"])}')
        print(f'  Complete runs:   {summary.get("complete_runs", 0)}')
        print(f'  Partial runs:    {summary.get("partial_runs", 0)}')
        print(f'  Raw-only runs:   {summary.get("raw_only_runs", 0)}')
        print(f'  Manifest rows:   {len(data["manifest"])}')
        if data['metadata']:
            print(f'  Branch:          {data["metadata"].get("branch", "N/A")}')
            print(f'  Scheduler:       {data["metadata"].get("sched", "N/A")}')
            print(f'  CPUs:            {data["metadata"].get("cpus", "N/A")}')
        else:
            print('  Branch:          N/A')
            print('  Scheduler:       N/A')
            print('  CPUs:            N/A')
        print(f'  Aggregated tests: {len(data["results"])}')


def main():
    print('=' * 68)
    print('xv6 Performance Test Log Validator')
    print('=' * 68)

    baseline = load_branch_dataset('baseline')
    testing = load_branch_dataset('testing')

    all_errors = []
    all_warnings = []

    for branch_name, dataset in (('baseline', baseline), ('testing', testing)):
        errors, warnings = validate_metadata(dataset, branch_name)
        all_errors.extend(errors)
        all_warnings.extend(warnings)
        errors, warnings = validate_results(dataset, branch_name)
        all_errors.extend(errors)
        all_warnings.extend(warnings)

    errors, warnings = validate_consistency(baseline, testing)
    all_errors.extend(errors)
    all_warnings.extend(warnings)
    all_warnings.extend(validate_scheduler_resolution(testing))

    print_summary(baseline, testing)

    print()
    print('=' * 68)
    print('VALIDATION RESULT')
    print('=' * 68)

    if all_errors:
        print(f'\n[ERRORS] {len(all_errors)}')
        for item in all_errors:
            print(f'  - {item}')

    if all_warnings:
        print(f'\n[WARNINGS] {len(all_warnings)}')
        for item in all_warnings:
            print(f'  - {item}')

    actions = recommend_actions(baseline, testing)
    if actions:
        print(f'\n[RECOMMENDED ACTIONS] {len(actions)}')
        for item in actions:
            print(f'  - {item}')

    if all_errors:
        print(f'\n[FAILED] {len(all_errors)} errors, {len(all_warnings)} warnings')
        return 1

    if all_warnings:
        print(f'\n[OK] No errors, but {len(all_warnings)} warnings')
    else:
        print('\n[OK] All validations passed!')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
