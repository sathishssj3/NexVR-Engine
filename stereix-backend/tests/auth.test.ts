import bcrypt from 'bcryptjs';
import jwt from 'jsonwebtoken';

jest.mock('../src/common/config.js', () => ({
  config: {
    JWT_SECRET: 'test-jwt-secret-auth-test-suite',
    JWT_REFRESH_SECRET: 'test-refresh-secret-auth-test-suite',
    JWT_EXPIRES_IN: '15m',
    JWT_REFRESH_EXPIRES_IN: '7d',
  },
}));

const mockDb = {
  user: {
    findFirst: jest.fn(),
    findUnique: jest.fn(),
    create: jest.fn(),
    update: jest.fn(),
  },
  refreshToken: {
    create: jest.fn(),
    findUnique: jest.fn(),
    findFirst: jest.fn(),
    update: jest.fn(),
    updateMany: jest.fn(),
    delete: jest.fn(),
    deleteMany: jest.fn(),
  },
};

jest.mock('../src/common/db.js', () => ({
  db: mockDb,
}));

jest.mock('../src/common/logger.js', () => ({
  logger: {
    info: jest.fn(),
    warn: jest.fn(),
    error: jest.fn(),
  },
}));

import { AuthService } from '../src/services/auth/auth.service.js';
import { requireAdmin, requireRole } from '../src/services/auth/auth.middleware.js';
import { ConflictError, UnauthorizedError, ForbiddenError } from '../src/common/errors.js';
import type { Request, Response, NextFunction } from 'express';

describe('AuthService', () => {
  let authService: AuthService;

  beforeEach(() => {
    jest.clearAllMocks();
    authService = new AuthService();
  });

  describe('register', () => {
    it('creates a new user with hashed password and returns tokens', async () => {
      mockDb.user.findFirst.mockResolvedValue(null);
      mockDb.user.create.mockResolvedValue({
        id: 'usr_001',
        username: 'vrgamer',
        email: 'vrgamer@example.test',
        role: 'USER',
        tier: 'FREE',
        createdAt: new Date(),
      });
      mockDb.refreshToken.create.mockResolvedValue({ id: 'tok_001' });

      const result = await authService.register('vrgamer', 'vrgamer@example.test', 'SecurePass123!');

      expect(mockDb.user.findFirst).toHaveBeenCalledWith({
        where: { OR: [{ email: 'vrgamer@example.test' }, { username: 'vrgamer' }] },
      });
      expect(mockDb.user.create).toHaveBeenCalled();
      expect(result.user.id).toBe('usr_001');
      expect(result.accessToken).toBeDefined();
      expect(result.refreshToken).toBeDefined();
    });

    it('rejects registration when username or email is already taken', async () => {
      mockDb.user.findFirst.mockResolvedValue({ id: 'existing_usr' });

      await expect(
        authService.register('existing', 'existing@example.test', 'pass')
      ).rejects.toThrow(ConflictError);
      expect(mockDb.user.create).not.toHaveBeenCalled();
    });
  });

  describe('login', () => {
    it('authenticates valid credentials and returns user payload and tokens', async () => {
      const passwordHash = await bcrypt.hash('CorrectPassword', 10);
      mockDb.user.findFirst.mockResolvedValue({
        id: 'usr_002',
        username: 'tester',
        email: 'tester@example.test',
        passwordHash,
        role: 'USER',
        tier: 'PRO',
      });
      mockDb.refreshToken.create.mockResolvedValue({ id: 'tok_002' });

      const result = await authService.login('tester@example.test', 'CorrectPassword');

      expect(result.user.id).toBe('usr_002');
      expect(result.user.email).toBe('tester@example.test');
      expect(result.accessToken).toBeDefined();
      expect(result.refreshToken).toBeDefined();
    });

    it('rejects login with unknown email/username', async () => {
      mockDb.user.findFirst.mockResolvedValue(null);

      await expect(
        authService.login('unknown@example.test', 'pass')
      ).rejects.toThrow(UnauthorizedError);
    });

    it('rejects login with incorrect password', async () => {
      const passwordHash = await bcrypt.hash('CorrectPassword', 10);
      mockDb.user.findFirst.mockResolvedValue({
        id: 'usr_003',
        username: 'tester',
        email: 'tester@example.test',
        passwordHash,
        role: 'USER',
        tier: 'FREE',
      });

      await expect(
        authService.login('tester@example.test', 'WrongPassword')
      ).rejects.toThrow(UnauthorizedError);
    });
  });

  describe('refreshToken', () => {
    it('rotates refresh token atomically when valid', async () => {
      mockDb.refreshToken.updateMany.mockResolvedValue({ count: 1 });
      mockDb.refreshToken.findFirst.mockResolvedValue({
        id: 'tok_004',
        user: {
          id: 'usr_004',
          email: 'rotate@example.test',
          role: 'USER',
          tier: 'PRO',
        },
      });
      mockDb.refreshToken.create.mockResolvedValue({ id: 'tok_new' });

      const res = await authService.refreshToken('valid_refresh_token');

      expect(mockDb.refreshToken.updateMany).toHaveBeenCalledWith({
        where: {
          token: 'valid_refresh_token',
          revoked: false,
          expiresAt: expect.any(Object),
        },
        data: { revoked: true },
      });
      expect(res.accessToken).toBeDefined();
      expect(res.refreshToken).toBeDefined();
    });

    it('rejects refresh token when token is already revoked or expired (race mitigation)', async () => {
      mockDb.refreshToken.updateMany.mockResolvedValue({ count: 0 });

      await expect(
        authService.refreshToken('already_revoked_token')
      ).rejects.toThrow(UnauthorizedError);
    });
  });

  describe('role-based authorization middleware', () => {
    const res = { status: jest.fn().mockReturnThis(), json: jest.fn() } as unknown as Response;

    it('requireAdmin allows users with ADMIN role', () => {
      const req = { user: { id: 'admin_1', role: 'ADMIN' } } as unknown as Request;
      const next = jest.fn();

      requireAdmin(req, res, next);
      expect(next).toHaveBeenCalledTimes(1);
    });

    it('requireAdmin denies users with USER role', () => {
      const req = { user: { id: 'user_1', role: 'USER' } } as unknown as Request;
      const next = jest.fn();

      expect(() => requireAdmin(req, res, next)).toThrow(ForbiddenError);
      expect(next).not.toHaveBeenCalled();
    });

    it('requireRole denies unauthenticated request without req.user', () => {
      const req = {} as unknown as Request;
      const next = jest.fn();

      expect(() => requireRole('PRO')(req, res, next)).toThrow(UnauthorizedError);
      expect(next).not.toHaveBeenCalled();
    });
  });
});
