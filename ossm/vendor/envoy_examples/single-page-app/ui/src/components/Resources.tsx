import {Button, Table} from '@chakra-ui/react'
import React from "react"
import {toaster} from "./ui/toast"

import {withAuth, withData} from "../hoc"
import {
  IActionState,
  IDataPayload,
  IRepoInfo,
  ITableResourceProps,
  IUserLogin} from "../@types/app"

class BaseResources<T> extends React.Component<ITableResourceProps<T>, IActionState> {

  constructor(props: ITableResourceProps<T>) {
    super(props)
    this.state = {errorMessage: '', isLoading: false}
  }

  render() {
    const {data, name, headers: Headers, row: Row, user} = this.props
    const rows = data.state.data?.[name]
    const {user: userData} = user.state || {}
    const {login} = userData || {}

    if (!login) {
      return ''
    }

    if (!rows || !Array.isArray(rows)) {
      return (
        <Button
          variant="subtle"
          onClick={() => this.updateResources()}>
          Update {name}
        </Button>)
    }

    return (
      <>
        <Button
          variant="subtle"
          onClick={() => this.updateResources()}>
          Update {name}
        </Button>
        <Table.Root striped>
          <Table.Header>
            <Table.Row>
              <Headers />
            </Table.Row>
          </Table.Header>
          <Table.Body>
            {rows.map((resource, index: number) => (
              <Table.Row key={index}>
                <Row resource={resource as T} />
              </Table.Row>
            ))}
          </Table.Body>
        </Table.Root>
    </>
    )
  }

  updateResources = async () => {
    const {data, name, user} = this.props
    const {dispatch} = data
    const {proxy_url, user: userData} = user.state
    if (!userData) {
      return
    }
    const {login} = userData
    try {
      const response = await fetch(`${proxy_url}/users/${login}/${name}`)
      const resources = await response.json()
      const payload: IDataPayload = {}
      payload[name] = resources
      dispatch({
        type: 'UPDATE',
        payload,
      })
      toaster.create({
        description: `Updated: ${name}`,
        type: "info",
        closable: true,
        duration: 3000,
      })
    } catch (error) {
      const e = error as Record<'message', string>
      this.setState({
        isLoading: false,
        errorMessage: `Sorry! Fetching ${name} failed\n${e.message}`,
      })
    }
 }
}

const BaseResourcesWithProps = (props: ITableResourceProps<IRepoInfo | IUserLogin>) => {
  return <BaseResources {...props} />
}

export const Resources = withAuth(withData(BaseResourcesWithProps as React.ComponentType))
