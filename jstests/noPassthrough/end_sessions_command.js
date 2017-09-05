(function() {
    "use script";

    var res;
    var refresh = {refreshLogicalSessionCacheNow: 1};
    var startSession = {startSession: 1};

    // Start up a standalone server.
    var conn = MongoRunner.runMongod({nojournal: ""});
    var admin = conn.getDB("admin");

    // Trigger an initial refresh, as a sanity check.
    res = admin.runCommand(refresh);
    assert.commandWorked(res, "failed to refresh");

    var sessions = [];
    for (var i = 0; i < 20; i++) {
        res = admin.runCommand(startSession);
        assert.commandWorked(res, "unable to start session");
        sessions.push(res);
    }

    res = admin.runCommand(refresh);
    assert.commandWorked(res, "failed to refresh");

    assert.eq(admin.system.sessions.count(), 20, "refresh should have written 20 session records");

    var endSessionsIds = [];
    for (var i = 0; i < 10; i++) {
        endSessionsIds.push(sessions[i].id);
    }
    res = admin.runCommand({endSessions: endSessionsIds});
    assert.commandWorked(res, "failed to end sessions");

    res = admin.runCommand(refresh);
    assert.commandWorked(res, "failed to refresh");

    assert.eq(admin.system.sessions.count(),
              10,
              "endSessions and refresh should result in 10 remaining sessions");

}());
