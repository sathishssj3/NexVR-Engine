import type { NextFunction, Request, Response } from 'express';
import jwt from 'jsonwebtoken';

jest.mock('../src/common/config.js', () => ({
  config: { JWT_SECRET: 'test-only-secret-for-auth-regressions' },
}));
jest.mock('../src/common/db.js', () => ({
  db: { user: { update: jest.fn() } },
}));
jest.mock('../src/common/logger.js', () => ({ logger: { error: jest.fn() } }));

import { config } from '../src/common/config.js';
import { db } from '../src/common/db.js';
import { ForbiddenError, UnauthorizedError } from '../src/common/errors.js';
import { authenticate } from '../src/services/auth/auth.middleware.js';
import { userRouter } from '../src/services/user/user.routes.js';

type Handler = (req: Request, res: Response, next: NextFunction) => void | Promise<void>;
type Layer = {
  handle: Handler;
  route?: {
    path: string;
    methods: Record<string, boolean>;
    stack: Array<{ handle: Handler }>;
  };
};

const layers = (userRouter as unknown as { stack: Layer[] }).stack;
const authLayer = layers[0];
const tierRoute = layers.find((layer) => layer.route?.path === '/tier' && layer.route.methods.patch)?.route;
const response = { status: jest.fn().mockReturnThis(), json: jest.fn() } as unknown as Response;
const user = { id: 'user-123', email: 'user@example.test', tier: 'FREE' };

function request(authorization?: string): Request {
  return {
    headers: authorization ? { authorization } : {},
    body: { tier: 'PRO' },
  } as unknown as Request;
}

function bearerToken(role: string, secret = config.JWT_SECRET, expiresIn = '15m'): string {
  return `Bearer ${jwt.sign({ ...user, role }, secret, { expiresIn: expiresIn as jwt.SignOptions['expiresIn'] })}`;
}

beforeEach(() => {
  jest.clearAllMocks();
});

describe('access token verification', () => {
  it('accepts a valid signed token and attaches its claims', () => {
    const req = request(bearerToken('ADMIN'));
    const next = jest.fn();

    authenticate(req, response, next);

    expect(req.user).toMatchObject({ ...user, role: 'ADMIN' });
    expect(next).toHaveBeenCalledTimes(1);
  });

  it.each([
    ['missing', undefined],
    ['malformed', 'Basic not-a-token'],
    ['bad signature', bearerToken('ADMIN', 'different-test-secret')],
    ['expired', bearerToken('ADMIN', config.JWT_SECRET, '-1s')],
  ])('rejects a %s token', (_case, authorization) => {
    const req = request(authorization);
    const next = jest.fn();

    expect(() => authenticate(req, response, next)).toThrow(UnauthorizedError);
    expect(req.user).toBeUndefined();
    expect(next).not.toHaveBeenCalled();
  });
});

describe('PATCH /user/tier authorization', () => {
  it.each(['USER', 'DEVELOPER'])('rejects %s without updating a tier', (role) => {
    const req = request(bearerToken(role));
    const next = jest.fn();

    expect(authLayer.handle).toBe(authenticate);
    expect(tierRoute?.stack).toHaveLength(2);
    authLayer.handle(req, response, next);
    expect(next).toHaveBeenCalledTimes(1);
    next.mockClear();

    expect(() => tierRoute!.stack[0].handle(req, response, next)).toThrow(ForbiddenError);
    expect(next).not.toHaveBeenCalled();
    expect(db.user.update).not.toHaveBeenCalled();
  });

  it('allows ADMIN and updates the validated tier', async () => {
    const updatedUser = { ...user, role: 'ADMIN', tier: 'PRO' };
    (db.user.update as unknown as jest.Mock).mockResolvedValue(updatedUser);
    const req = request(bearerToken('ADMIN'));
    const next = jest.fn();

    expect(authLayer.handle).toBe(authenticate);
    expect(tierRoute?.stack).toHaveLength(2);
    authLayer.handle(req, response, next);
    expect(next).toHaveBeenCalledTimes(1);
    next.mockClear();
    tierRoute!.stack[0].handle(req, response, next);
    expect(next).toHaveBeenCalledTimes(1);
    next.mockClear();
    await tierRoute!.stack[1].handle(req, response, next);

    expect(next).not.toHaveBeenCalled();
    expect(db.user.update).toHaveBeenCalledWith(expect.objectContaining({
      where: { id: user.id },
      data: { tier: 'PRO' },
    }));
    expect(response.status).toHaveBeenCalledWith(200);
    expect(response.json).toHaveBeenCalledWith({ status: 'success', data: updatedUser });
  });
});
