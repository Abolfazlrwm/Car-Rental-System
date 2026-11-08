#ifndef PERMISSIONS_H
#define PERMISSIONS_H

#include <string>
#include <vector>

/**
 * ============================================================
 *  Permissions — the fine-grained role matrix   (v2.5.0)
 * ============================================================
 *  Before v2.5.0 the CLI menus were the ONLY guard between a
 *  role and a privileged operation. The matrix below makes the
  *  rules explicit and lets the RentalManager facade re-check
 *  them on every manager-exclusive call, so a future client
 *  (GUI, script, API) inherits the same policy for free.
 *
 *  Manager: everything. Staff: the front-desk trio (deliver,
 *  return, maintenance). Customer: no privileged calls.
 * ============================================================
 */
enum class Permission
{
    MANAGE_USERS,      // create Staff/Manager accounts, block & unblock
    EXPORT_REPORTS,    // write reports/*.csv and reports/report.json
    VIEW_ANALYTICS,    // fleet analytics dashboard
    VIEW_AUDIT_LOG,    // read the audit trail
    HANDLE_DELIVERIES, // hand a reserved car to the customer
    PROCESS_RETURNS,   // process returns, penalties and invoices
    SEND_MAINTENANCE,  // maintenance transitions and logs
};

/// Stable kebab-case id, also used in audit details.
inline const char *permissionName(Permission p)
{
    switch (p)
    {
    case Permission::MANAGE_USERS:
        return "manage-users";
    case Permission::EXPORT_REPORTS:
        return "export-reports";
    case Permission::VIEW_ANALYTICS:
        return "view-analytics";
    case Permission::VIEW_AUDIT_LOG:
        return "view-audit-log";
    case Permission::HANDLE_DELIVERIES:
        return "handle-deliveries";
    case Permission::PROCESS_RETURNS:
        return "process-returns";
    case Permission::SEND_MAINTENANCE:
        return "send-maintenance";
    }
    return "unknown";
}

/// The full permission set of a role ("Customer" / "Staff" /
/// "Manager"), display order. Unknown roles get an empty set.
inline std::vector<Permission> rolePermissions(const std::string &role)
{
    if (role == "Manager")
        return {Permission::MANAGE_USERS,       Permission::EXPORT_REPORTS,
                Permission::VIEW_ANALYTICS,     Permission::VIEW_AUDIT_LOG,
                Permission::HANDLE_DELIVERIES,  Permission::PROCESS_RETURNS,
                Permission::SEND_MAINTENANCE};
    if (role == "Staff")
        return {Permission::HANDLE_DELIVERIES, Permission::PROCESS_RETURNS,
                Permission::SEND_MAINTENANCE};
    return {};
}

/// Matrix lookup used by the facade before every privileged call.
inline bool roleHasPermission(const std::string &role, Permission p)
{
    for (const Permission q : rolePermissions(role))
        if (q == p)
            return true;
    return false;
}

#endif // PERMISSIONS_H
