import {useContext, useState} from "react"
import {FiChevronDown} from 'react-icons/fi';
import {
  Button,
  CloseButton,
  Dialog,
  Flex,
  Image,
  Menu,
  Portal,
  Table,
  Text,
} from '@chakra-ui/react'
import {AuthContext} from "../context"
import {IUser, TAuthContext} from "../@types/app"
import {AuthProviders} from "../providers.tsx"
import {toaster} from "./ui/toast"
import {IComponentWithUserInfoProp, IUserDialogProps, IUserMenuProps} from "../@types/app"

export const UserInfoTable = (props: IComponentWithUserInfoProp) => {
  const {user} = props
  const {
    followers,
    following,
    public_repos} = user as IUser
  return (
    <Table.Root striped>
      <Table.Header>
        <Table.Row>
          <Table.ColumnHeader>metric</Table.ColumnHeader>
          <Table.ColumnHeader>count</Table.ColumnHeader>
        </Table.Row>
      </Table.Header>
      <Table.Body>
        <Table.Row>
          <Table.Cell>Repos</Table.Cell>
          <Table.Cell>{public_repos}</Table.Cell>
        </Table.Row>
        <Table.Row>
          <Table.Cell>Followers</Table.Cell>
          <Table.Cell>{followers}</Table.Cell>
        </Table.Row>
        <Table.Row>
          <Table.Cell>Following</Table.Cell>
          <Table.Cell>{following}</Table.Cell>
        </Table.Row>
      </Table.Body>
    </Table.Root>)
}

export const UserInfoDialog = (props: IUserDialogProps) => {
  const {open, setOpen, user} = props
  const {
    avatar_url,
    login} = user as IUser
  return (
    <Dialog.Root open={open} onOpenChange={({open}) => setOpen(open)}>
      <Portal>
        <Dialog.Backdrop />
        <Dialog.Positioner>
          <Dialog.Content>
            <Dialog.Header>
              <Dialog.Title>
                <Flex align="center">
                  <Image
                    boxSize='2rem'
                    borderRadius='full'
                    src={avatar_url}
                    alt="Avatar"
                    mr='12px'
                  />
                  <Text>{login}</Text>
                </Flex>
              </Dialog.Title>
            </Dialog.Header>
            <Dialog.Body>
              <UserInfoTable user={user} />
            </Dialog.Body>
            <Dialog.Footer>
              <Dialog.ActionTrigger asChild>
                <Button variant="subtle">Close</Button>
              </Dialog.ActionTrigger>
            </Dialog.Footer>
            <Dialog.CloseTrigger asChild>
              <CloseButton />
            </Dialog.CloseTrigger>
          </Dialog.Content>
        </Dialog.Positioner>
      </Portal>
    </Dialog.Root>)
}

export const UserMenuDropdown = (props: IUserMenuProps) => {
  const {onInfoClick, handleLogout, open, setOpen, user} = props
  const {
    avatar_url = '...',
    login = '...'} = user as IUser || {}
  return (
    <Menu.Root open={open} onOpenChange={({open}) => setOpen(open)}>
      <Menu.Trigger asChild>
        <Button variant="subtle">
          <Flex align="center" gap="2">
            <Image boxSize="2rem" borderRadius="full" src={avatar_url} alt={login} />
            <Text>{login}</Text>
            <FiChevronDown />
          </Flex>
        </Button>
      </Menu.Trigger>
      <Portal>
        <Menu.Positioner>
          <Menu.Content>
            <Menu.Item value="info" onSelect={onInfoClick}>Info</Menu.Item>
            <Menu.Item value="logout" onSelect={handleLogout}>Logout</Menu.Item>
          </Menu.Content>
        </Menu.Positioner>
      </Portal>
    </Menu.Root>)
}

export const UserMenu = () => {
  const {state, dispatch} = useContext(AuthContext) as TAuthContext
  const {authenticating, isLoggedIn, provider, user} = state
  const authProvider = AuthProviders[provider]
  const handleLogin = async () => {
    // This is intercepted and redirected by Envoy
    window.location.href = '/login'
  }
  const [menuOpen, setMenuOpen] = useState(false);
  const [dialogOpen, setDialogOpen] = useState(false);
  if (!isLoggedIn) {
    const {icon: Icon, name: providerName} = authProvider
    const loginText = authenticating ? 'Logging in' : `Login to ${providerName}`;
    return (
      <Button variant="subtle" onClick={() => handleLogin()}>
        <Flex align="center">
          <Icon />
          <Text>{loginText}</Text>
        </Flex>
      </Button>)
  }
  const onInfoClick = () => {
    setMenuOpen(false)
    setDialogOpen(true)
  };
  const handleLogout = async () => {
    const response = await fetch('/logout')
    if (response.status === 200) {
      await dispatch({
        type: "LOGOUT",
      })
    } else {
      toaster.create({
        description: `Logout failed: ${response.statusText}`,
        type: "error",
        closable: true,
        duration: 3000,
      })
    }

  }
  return (
    <>
      <UserMenuDropdown
        handleLogout={handleLogout}
        onInfoClick={onInfoClick}
        open={menuOpen}
        setOpen={setMenuOpen}
        user={user} />
      <UserInfoDialog
        open={dialogOpen}
        setOpen={setDialogOpen}
        user={user} />
    </>
  );
}
