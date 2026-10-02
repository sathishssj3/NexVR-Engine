import bcrypt from 'bcryptjs';
import jwt from 'jsonwebtoken';
import { db } from '../../common/db.js';
import { config } from '../../common/config.js';
import { ConflictError, UnauthorizedError, NotFoundError } from '../../common/errors.js';
import { AuthenticatedUser } from './auth.middleware.js';

export class AuthService {
  async register(username: string, email: string, password: string) {
    const existingUser = await db.user.findFirst({
      where: { OR: [{ email }, { username }] },
    });

    if (existingUser) {
      throw new ConflictError('Username or email already registered');
    }

    const passwordHash = await bcrypt.hash(password, 12);
    const user = await db.user.create({
      data: {
        username,
        email,
        passwordHash,
      },
      select: {
        id: true,
        username: true,
        email: true,
        role: true,
        tier: true,
        createdAt: true,
      },
    });

    const tokens = await this.generateTokens(user);
    return { user, ...tokens };
  }

  async login(emailOrUsername: string, password: string) {
    const user = await db.user.findFirst({
      where: {
        OR: [{ email: emailOrUsername }, { username: emailOrUsername }],
      },
    });

    if (!user) {
      throw new UnauthorizedError('Invalid email or password');
    }

    const isValid = await bcrypt.compare(password, user.passwordHash);
    if (!isValid) {
      throw new UnauthorizedError('Invalid email or password');
    }

    const userPayload: AuthenticatedUser = {
      id: user.id,
      email: user.email,
      role: user.role,
      tier: user.tier,
    };

    const tokens = await this.generateTokens(userPayload);
    return {
      user: {
        id: user.id,
        username: user.username,
        email: user.email,
        role: user.role,
        tier: user.tier,
      },
      ...tokens,
    };
  }

  async refreshToken(token: string) {
    const storedToken = await db.refreshToken.findUnique({
      where: { token },
      include: { user: true },
    });

    if (!storedToken || storedToken.revoked || storedToken.expiresAt < new Date()) {
      throw new UnauthorizedError('Invalid or expired refresh token');
    }

    const userPayload: AuthenticatedUser = {
      id: storedToken.user.id,
      email: storedToken.user.email,
      role: storedToken.user.role,
      tier: storedToken.user.tier,
    };

    // Rotate refresh token
    await db.refreshToken.update({
      where: { id: storedToken.id },
      data: { revoked: true },
    });

    return await this.generateTokens(userPayload);
  }

  async logout(token: string) {
    await db.refreshToken.updateMany({
      where: { token },
      data: { revoked: true },
    });
  }

  private async generateTokens(user: AuthenticatedUser | { id: string; email: string; role: string; tier: string }) {
    const accessToken = jwt.sign(
      { id: user.id, email: user.email, role: user.role, tier: user.tier },
      config.JWT_SECRET,
      { expiresIn: config.JWT_EXPIRES_IN as any }
    );

    const refreshTokenString = `${user.id}.${jwt.sign({ id: user.id }, config.JWT_SECRET, { expiresIn: '7d' })}`;
    const expiresAt = new Date(Date.now() + 7 * 24 * 60 * 60 * 1000);

    await db.refreshToken.create({
      data: {
        token: refreshTokenString,
        userId: user.id,
        expiresAt,
      },
    });

    return { accessToken, refreshToken: refreshTokenString };
  }
}
