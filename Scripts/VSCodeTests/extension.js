// Copyright HoowanDev 2026 all rights reserved.

// Puts the project's Unreal automation tests in VS Code's Testing view.
// Tests are found by reading the IMPLEMENT_*_AUTOMATION_TEST / DEFINE_SPEC macros in Source/, and run headless through
// UnrealEditor-Cmd ("Automation RunTests ...;Quit"). Results come from the index.json report the run exports, and the
// log is streamed into the test output so tests show as running and passing while the run is still going.

const vscode = require("vscode");
const cp = require("child_process");
const fs = require("fs");
const path = require("path");

const TestMacro =
	/\b(?:IMPLEMENT_(?:CUSTOM_)?(?:SIMPLE|COMPLEX)_AUTOMATION_TEST(?:_PRIVATE)?|IMPLEMENT_NETWORKED_AUTOMATION_TEST|BEGIN_DEFINE_SPEC|DEFINE_SPEC|BEGIN_CUSTOM_COMPLEX_AUTOMATION_TEST)\s*\(\s*\w+\s*,\s*(?:\w+\s*,\s*)?"([^"]+)"/g;
const TestStartedLine = /Test Started\. Name=\{.*?\} Path=\{(.+?)\}/;
const TestCompletedLine = /Test Completed\. Result=\{(\w+)\} Name=\{.*?\} Path=\{(.+?)\}/;

/** @type {vscode.TestController} */
let controller;
/** Folder holding the .uproject, and the .uproject itself. */
let project;
/** Items that are tests (as opposed to groups), by full test path. */
const testsByPath = new Map();
/** Tests created from results rather than source, such as the cases of a complex test or the Its of a spec. */
const dynamicTests = new WeakSet();

function activate(context) {
	controller = vscode.tests.createTestController("fellowshipTests", "Fellowship");
	context.subscriptions.push(controller);

	controller.refreshHandler = () => discover();
	controller.createRunProfile("Run", vscode.TestRunProfileKind.Run, (request, token) => runTests(request, token, false), true);
	controller.createRunProfile("Debug", vscode.TestRunProfileKind.Debug, (request, token) => runTests(request, token, true), true);

	const watcher = vscode.workspace.createFileSystemWatcher("**/Source/**/*.cpp");
	let pending;
	const rediscover = () => {
		clearTimeout(pending);
		pending = setTimeout(() => discover(), 500);
	};
	watcher.onDidChange(rediscover);
	watcher.onDidCreate(rediscover);
	watcher.onDidDelete(rediscover);
	context.subscriptions.push(watcher);

	discover();
}

function settings() {
	const config = vscode.workspace.getConfiguration("fellowshipTests");
	return {
		engineDir: config.get("engineDir"),
		buildBeforeRun: config.get("buildBeforeRun"),
		extraArgs: config.get("extraArgs") || [],
	};
}

async function findProject() {
	for (const folder of vscode.workspace.workspaceFolders || []) {
		const found = await vscode.workspace.findFiles(new vscode.RelativePattern(folder, "*.uproject"), null, 1);
		if (found.length > 0) {
			return { folder, file: found[0].fsPath, dir: folder.uri.fsPath };
		}
	}
	return undefined;
}

// Discovery

async function discover() {
	project = await findProject();
	if (!project) {
		return;
	}

	const files = await vscode.workspace.findFiles(new vscode.RelativePattern(project.folder, "Source/**/*.cpp"));
	const found = [];
	for (const uri of files) {
		const text = (await vscode.workspace.fs.readFile(uri)).toString();
		if (!text.includes("AUTOMATION_TEST") && !text.includes("DEFINE_SPEC")) {
			continue;
		}
		// Blank out line comments so commented-out tests aren't listed, keeping offsets (and so line numbers) intact.
		const code = text.replace(/\/\/[^\n]*/g, (comment) => " ".repeat(comment.length));
		for (const match of code.matchAll(TestMacro)) {
			const line = code.slice(0, match.index).split("\n").length - 1;
			found.push({ path: match[1], uri, line });
		}
	}

	testsByPath.clear();
	const roots = new Map();
	for (const test of found.sort((a, b) => a.path.localeCompare(b.path))) {
		const item = ensureItem(test.path, roots, test.uri);
		item.range = new vscode.Range(test.line, 0, test.line, 0);
		testsByPath.set(test.path, item);
	}
	controller.items.replace([...roots.values()]);
}

/** Returns the item for a dotted path, creating it and the groups above it. */
function ensureItem(fullPath, roots, uri) {
	const parts = fullPath.split(".");
	let collection = null;
	let item;
	for (let i = 0; i < parts.length; i++) {
		const id = parts.slice(0, i + 1).join(".");
		const existing = collection ? collection.get(id) : roots.get(id);
		item = existing || controller.createTestItem(id, parts[i], i === parts.length - 1 ? uri : undefined);
		if (!existing) {
			if (collection) {
				collection.add(item);
			} else {
				roots.set(id, item);
			}
		}
		collection = item.children;
	}
	return item;
}

function isTest(item) {
	return testsByPath.get(item.id) === item;
}

// Running

/**
 * Works out which tests a request covers and the RunTests filters that select them.
 * A whole group is one "^Group." filter; a group with exclusions is broken down into its children.
 */
function plan(request) {
	const excluded = new Set((request.exclude || []).map((item) => item.id));
	const hasExclusion = (item) => {
		if (excluded.has(item.id)) {
			return true;
		}
		let found = false;
		item.children.forEach((child) => (found = found || hasExclusion(child)));
		return found;
	};

	const filters = new Set();
	const tests = new Set();
	const addTests = (item) => {
		if (excluded.has(item.id)) {
			return;
		}
		if (isTest(item) || dynamicTests.has(item)) {
			tests.add(item);
		}
		item.children.forEach(addTests);
	};
	const visit = (item) => {
		if (excluded.has(item.id)) {
			return;
		}
		if (dynamicTests.has(item)) {
			// Cases of a complex test can't be run on their own, so run the whole test.
			let owner = item.parent;
			while (owner && !isTest(owner)) {
				owner = owner.parent;
			}
			if (owner) {
				filters.add(`^${owner.id}$`);
				addTests(owner);
			}
			return;
		}
		if (!hasExclusion(item)) {
			if (isTest(item)) {
				filters.add(`^${item.id}$`);
			}
			if (item.children.size > 0) {
				filters.add(`^${item.id}.`);
			}
			addTests(item);
			return;
		}
		if (isTest(item)) {
			filters.add(`^${item.id}$`);
			tests.add(item);
		}
		item.children.forEach(visit);
	};

	if (request.include) {
		request.include.forEach(visit);
	} else {
		controller.items.forEach(visit);
	}
	return { filters: [...filters], tests: [...tests] };
}

async function runTests(request, token, debug) {
	if (!project) {
		await discover();
	}
	const run = controller.createTestRun(request);
	try {
		if (!project) {
			run.appendOutput("No .uproject found in the workspace.\r\n");
			return;
		}
		const { filters, tests } = plan(request);
		if (filters.length === 0) {
			return;
		}
		tests.forEach((test) => run.enqueued(test));

		const options = settings();
		const configuration = debug ? "DebugGame" : "Development";
		if (options.buildBeforeRun) {
			const built = await build(run, token, options, configuration);
			if (token.isCancellationRequested) {
				return;
			}
			if (!built) {
				const message = new vscode.TestMessage("The build failed, so the tests didn't run. See the test output for the errors.");
				tests.forEach((test) => run.errored(test, message));
				return;
			}
		}

		const reportDir = path.join(project.dir, "Saved", "Automation", "VSCode");
		fs.rmSync(path.join(reportDir, "index.json"), { force: true });
		const args = [
			project.file,
			`-ExecCmds=Automation RunTests ${filters.join("+")};Quit`,
			"-unattended",
			"-nullrhi",
			"-nopause",
			"-nosplash",
			"-stdout",
			`-ReportExportPath=${reportDir}`,
			...options.extraArgs,
		];

		const state = { started: new Set(), finished: new Set() };
		if (debug) {
			tests.forEach((test) => run.started(test));
			await debugEditor(options, args, token);
		} else {
			await runEditor(run, token, options, args, state);
		}
		if (token.isCancellationRequested) {
			return;
		}
		reportResults(run, tests, reportDir, state);
	} catch (error) {
		run.appendOutput(`${error && error.stack ? error.stack : error}\r\n`.replace(/\r?\n/g, "\r\n"));
	} finally {
		run.end();
	}
}

/** Runs the editor target's build. Returns false if it failed for any reason other than Live Coding. */
async function build(run, token, options, configuration) {
	const buildBat = path.join(options.engineDir, "Engine", "Build", "BatchFiles", "Build.bat");
	run.appendOutput(`Building ${path.basename(project.file, ".uproject")}Editor ${configuration}...\r\n`);
	let log = "";
	const code = await spawn(
		"cmd.exe",
		// /s strips the outer quotes and keeps the inner ones, so paths with spaces survive. The same as Node's shell: true.
		["/d", "/s", "/c", `""${buildBat}" ${path.basename(project.file, ".uproject")}Editor Win64 ${configuration} "${project.file}" -waitmutex"`],
		{ cwd: options.engineDir, windowsVerbatimArguments: true },
		token,
		(text) => {
			log += text;
			run.appendOutput(text.replace(/\r?\n/g, "\r\n"));
		}
	);
	if (code === 0) {
		return true;
	}
	if (/Live Coding/i.test(log)) {
		run.appendOutput(
			"\r\n*** The editor is open with Live Coding, so it can't be built from here. Running the tests on the binaries already on disk,\r\n" +
				"*** which don't include changes patched in by Live Coding. Close the editor, or compile from it, to test the latest code.\r\n\r\n"
		);
		return true;
	}
	return false;
}

/** Runs the tests headless, streaming the log into the run and marking tests as they start and pass. */
async function runEditor(run, token, options, args, state) {
	const exe = path.join(options.engineDir, "Engine", "Binaries", "Win64", "UnrealEditor-Cmd.exe");
	run.appendOutput(`\r\nRunning ${args[1].slice("-ExecCmds=".length)}\r\n`);
	let partial = "";
	await spawn(exe, args, { cwd: options.engineDir }, token, (text) => {
		const lines = (partial + text).split(/\r?\n/);
		partial = lines.pop();
		for (const line of lines) {
			const started = TestStartedLine.exec(line);
			const completed = TestCompletedLine.exec(line);
			const item = itemForPath((started || completed || [])[started ? 1 : 2]);
			if (started && item) {
				state.started.add(item.id);
				run.started(item);
			}
			if (completed && item && completed[1] === "Success") {
				// Failures wait for the report, which has their messages and locations.
				state.finished.add(item.id);
				run.passed(item);
			}
			run.appendOutput(line + "\r\n", undefined, item);
		}
	});
}

/** Runs the tests under the debugger and waits for the session to end. */
async function debugEditor(options, args, token) {
	const name = "Fellowship Tests";
	const ended = new Promise((resolve) => {
		const listener = vscode.debug.onDidTerminateDebugSession((session) => {
			if (session.name === name) {
				listener.dispose();
				resolve();
			}
		});
	});
	const started = await vscode.debug.startDebugging(project.folder, {
		name,
		type: "cppvsdbg",
		request: "launch",
		program: path.join(options.engineDir, "Engine", "Binaries", "Win64", "UnrealEditor-Win64-DebugGame-Cmd.exe"),
		args,
		cwd: options.engineDir,
		console: "internalConsole",
		visualizerFile: path.join(options.engineDir, "Engine", "Extras", "VisualStudioDebugging", "Unreal.natvis"),
		sourceFileMap: { "D:\\build\\++UE5\\Sync": options.engineDir },
	});
	if (!started) {
		throw new Error("Couldn't start the debugger. Is the C/C++ extension (ms-vscode.cpptools) installed?");
	}
	const cancelled = token.onCancellationRequested(() => vscode.debug.stopDebugging());
	await ended;
	cancelled.dispose();
}

function spawn(command, args, options, token, onOutput) {
	return new Promise((resolve, reject) => {
		const child = cp.spawn(command, args, { ...options, windowsHide: true });
		const cancelled = token.onCancellationRequested(() => {
			// Kill the whole tree: Build.bat runs UnrealBuildTool and the editor may start shader workers.
			cp.spawn("taskkill", ["/pid", String(child.pid), "/T", "/F"], { windowsHide: true });
		});
		child.stdout.on("data", (data) => onOutput(data.toString()));
		child.stderr.on("data", (data) => onOutput(data.toString()));
		child.on("error", (error) => {
			cancelled.dispose();
			reject(error);
		});
		child.on("close", (code) => {
			cancelled.dispose();
			resolve(code);
		});
	});
}

/** Finds the item for a test path from a run, creating one under its test for cases that aren't in the source. */
function itemForPath(fullPath) {
	if (!fullPath) {
		return undefined;
	}
	const known = testsByPath.get(fullPath);
	if (known) {
		return known;
	}
	const parts = fullPath.split(".");
	for (let i = parts.length - 1; i > 0; i--) {
		const owner = testsByPath.get(parts.slice(0, i).join("."));
		if (!owner) {
			continue;
		}
		let item = owner;
		for (let j = i; j < parts.length; j++) {
			const id = parts.slice(0, j + 1).join(".");
			let child = item.children.get(id);
			if (!child) {
				child = controller.createTestItem(id, parts[j], owner.uri);
				child.range = owner.range;
				dynamicTests.add(child);
				item.children.add(child);
			}
			item = child;
		}
		return item;
	}
	return undefined;
}

function reportResults(run, tests, reportDir, state) {
	const reportFile = path.join(reportDir, "index.json");
	let report;
	try {
		report = JSON.parse(fs.readFileSync(reportFile, "utf8").replace(/^\uFEFF/, ""));
	} catch {
		const message = new vscode.TestMessage(
			"The run ended without writing a report, so it probably crashed. See the test output for the log."
		);
		tests.forEach((test) => {
			if (!state.finished.has(test.id)) {
				run.errored(test, message);
			}
		});
		return;
	}

	const reported = new Set();
	for (const result of report.tests || []) {
		const item = itemForPath(result.fullTestPath);
		if (!item) {
			continue;
		}
		reported.add(item.id);
		const duration = typeof result.duration === "number" ? result.duration * 1000 : undefined;
		const entries = result.entries || [];
		for (const entry of entries) {
			if (entry.event && entry.event.type !== "Info") {
				run.appendOutput(`[${result.fullTestPath}] ${entry.event.type}: ${entry.event.message}\r\n`, undefined, item);
			}
		}

		if (result.state === "Success") {
			if (!state.finished.has(item.id)) {
				run.passed(item, duration);
			}
		} else if (result.state === "Fail") {
			const messages = entries
				.filter((entry) => entry.event && entry.event.type === "Error")
				.map((entry) => {
					const message = new vscode.TestMessage(entry.event.message);
					message.location = locationOf(entry, item);
					return message;
				});
			if (messages.length === 0) {
				messages.push(new vscode.TestMessage("The test failed without reporting an error."));
			}
			run.failed(item, messages, duration);
		} else if (result.state === "Skipped" || result.state === "NotRun") {
			run.skipped(item);
		} else {
			run.errored(item, new vscode.TestMessage(`The test ended in state '${result.state}'.`), duration);
		}
	}

	const missing = new vscode.TestMessage(
		"The run didn't include this test. If it's new, the binaries probably don't have it yet: build, then run again."
	);
	tests.forEach((test) => {
		if (!reported.has(test.id) && !state.finished.has(test.id) && !dynamicTests.has(test)) {
			run.errored(test, missing);
		}
	});
}

function locationOf(entry, item) {
	if (entry.filename && entry.lineNumber > 0 && fs.existsSync(entry.filename)) {
		return new vscode.Location(vscode.Uri.file(entry.filename), new vscode.Position(entry.lineNumber - 1, 0));
	}
	if (item.uri && item.range) {
		return new vscode.Location(item.uri, item.range);
	}
	return undefined;
}

function deactivate() {}

module.exports = { activate, deactivate };
