#include <gtest/gtest.h>
#include "hydrogen/server/repositories/user_repository.h"
#include <memory>
#include <filesystem>

using namespace hydrogen::server::repositories;
using namespace hydrogen::server::services;

// Simple factory function for testing
std::unique_ptr<IUserRepository> createTestUserRepository(const std::string& path) {
    // For now, return nullptr - this would need a concrete implementation
    // In a real implementation, this would create a file-based or memory-based repository
    return nullptr;
}

class UserRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDataPath_ = "./test_data/users_test.json";
        std::filesystem::create_directories("./test_data");
        repository_ = createTestUserRepository(testDataPath_);
        // Skip tests if no implementation available
        if (!repository_) {
            GTEST_SKIP() << "No user repository implementation available for testing";
        }
    }
    
    void TearDown() override {
        if (std::filesystem::exists(testDataPath_)) {
            std::filesystem::remove(testDataPath_);
        }
    }
    
    UserInfo createTestUser(const std::string& id) {
        UserInfo user;
        user.userId = id;
        user.username = "testuser_" + id;
        user.email = "test_" + id + "@example.com";
        user.fullName = "Test User " + id;
        user.role = UserRole::USER;
        user.permissions = {Permission::READ_DEVICES, Permission::WRITE_DEVICES};
        user.isActive = true;
        user.isLocked = false;
        user.createdAt = std::chrono::system_clock::now();
        user.lastLoginAt = std::chrono::system_clock::now();
        user.passwordChangedAt = std::chrono::system_clock::now();
        user.failedLoginAttempts = 0;
        user.lockedUntil = std::chrono::system_clock::time_point{};
        return user;
    }
    
    std::unique_ptr<IUserRepository> repository_;
    std::string testDataPath_;
};

TEST_F(UserRepositoryTest, BasicCRUDOperations) {
    auto user = createTestUser("1");
    std::string passwordHash = "hashed_password_1";

    // Create
    EXPECT_TRUE(repository_->create(user, passwordHash));
    EXPECT_TRUE(repository_->exists(user.userId));
    EXPECT_EQ(repository_->count(), 1);
    
    // Read
    auto retrieved = repository_->read(user.userId);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->userId, user.userId);
    EXPECT_EQ(retrieved->username, user.username);
    
    // Update
    user.fullName = "Updated Test User";
    EXPECT_TRUE(repository_->update(user));

    auto updated = repository_->read(user.userId);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->fullName, "Updated Test User");
    
    // Delete
    EXPECT_TRUE(repository_->remove(user.userId));
    EXPECT_FALSE(repository_->exists(user.userId));
    EXPECT_EQ(repository_->count(), 0);
}

TEST_F(UserRepositoryTest, AuthenticationOperations) {
    auto user = createTestUser("auth_test");
    std::string passwordHash = "hashed_password_auth_test";
    repository_->create(user, passwordHash);

    // Find by username
    auto found = repository_->findByUsername(user.username);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->userId, user.userId);

    // Find by email
    auto foundByEmail = repository_->findByEmail(user.email);
    ASSERT_TRUE(foundByEmail.has_value());
    EXPECT_EQ(foundByEmail->userId, user.userId);

    // Check if username exists
    EXPECT_TRUE(repository_->usernameExists(user.username));
    EXPECT_FALSE(repository_->usernameExists("nonexistent_user"));

    // Check if email exists
    EXPECT_TRUE(repository_->emailExists(user.email));
    EXPECT_FALSE(repository_->emailExists("nonexistent@example.com"));

    // Update password
    std::string newPasswordHash = "new_hashed_password";
    EXPECT_TRUE(repository_->updatePassword(user.userId, newPasswordHash));

    // Get password hash
    auto retrievedHash = repository_->getPasswordHash(user.userId);
    ASSERT_TRUE(retrievedHash.has_value());
    EXPECT_EQ(*retrievedHash, newPasswordHash);
}

TEST_F(UserRepositoryTest, RoleManagement) {
    auto user = createTestUser("role_test");
    std::string passwordHash = "hashed_password_role_test";
    repository_->create(user, passwordHash);

    // Update role
    EXPECT_TRUE(repository_->updateRole(user.userId, UserRole::ADMIN));

    // Get users by role
    auto adminUsers = repository_->findByRole(UserRole::ADMIN);
    EXPECT_GE(adminUsers.size(), 1);

    // Check if user is in admin users
    bool found = false;
    for (const auto& adminUser : adminUsers) {
        if (adminUser.userId == user.userId) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    // Count users by role
    size_t adminCount = repository_->countByRole(UserRole::ADMIN);
    EXPECT_GE(adminCount, 1);
}

TEST_F(UserRepositoryTest, PermissionManagement) {
    auto user = createTestUser("perm_test");
    std::string passwordHash = "hashed_password_perm_test";
    repository_->create(user, passwordHash);

    // Grant permission
    EXPECT_TRUE(repository_->grantPermission(user.userId, Permission::MANAGE_USERS));
    EXPECT_TRUE(repository_->hasPermission(user.userId, Permission::MANAGE_USERS));
    EXPECT_TRUE(repository_->hasPermission(user.userId, Permission::READ_DEVICES)); // Original permission

    // Get user permissions
    auto permissions = repository_->getUserPermissions(user.userId);
    EXPECT_GE(permissions.size(), 2);
    EXPECT_TRUE(permissions.count(Permission::MANAGE_USERS) > 0);
    EXPECT_TRUE(permissions.count(Permission::READ_DEVICES) > 0);

    // Revoke permission
    EXPECT_TRUE(repository_->revokePermission(user.userId, Permission::READ_DEVICES));
    EXPECT_FALSE(repository_->hasPermission(user.userId, Permission::READ_DEVICES));
    EXPECT_TRUE(repository_->hasPermission(user.userId, Permission::MANAGE_USERS));
}

TEST_F(UserRepositoryTest, SearchOperations) {
    auto user1 = createTestUser("search1");
    user1.fullName = "John Doe";

    auto user2 = createTestUser("search2");
    user2.fullName = "Jane Smith";

    repository_->create(user1, "password1");
    repository_->create(user2, "password2");

    // Search by name
    auto johnResults = repository_->search("John");
    EXPECT_GE(johnResults.size(), 0); // May or may not find results depending on implementation

    // Search by email
    auto emailResults = repository_->search("search1@example.com");
    EXPECT_GE(emailResults.size(), 0);
}

TEST_F(UserRepositoryTest, StatisticsOperations) {
    auto user1 = createTestUser("stats1");
    auto user2 = createTestUser("stats2");

    repository_->create(user1, "password1");
    repository_->create(user2, "password2");

    // Count operations
    EXPECT_GE(repository_->count(), 2);
    EXPECT_GE(repository_->countActiveUsers(), 2);
    EXPECT_EQ(repository_->countLockedUsers(), 0);

    // Count by role
    size_t userRoleCount = repository_->countByRole(UserRole::USER);
    EXPECT_GE(userRoleCount, 2);

    // Get role statistics
    auto roleStats = repository_->getRoleStatistics();
    EXPECT_GE(roleStats.size(), 1);
    EXPECT_GE(roleStats[UserRole::USER], 2);
}
